/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "comm_utils.h"
#include "comm_connectivity.h"

#ifdef USE_SHM
#include "comm_channel.h"
#endif


static ssize_t plcSocketRecv(plcConn *conn, void *ptr, size_t len);
static ssize_t plcSocketSend(plcConn *conn, const void *ptr, size_t len);
static int plcBufferMaybeFlush (plcConn *conn, bool isForse);
static int plcBufferMaybeReset (plcConn *conn, int bufType);
static int plcBufferMaybeResize (plcConn *conn, int bufType, size_t bufAppend);

#ifdef USE_SHM
static void write_buf_head_room(plcBuffer *buf)
{
	int *data_info = (int *)((char *) buf->data - 2 * sizeof(int));

	data_info[0] = buf->pStart;
	data_info[1] = buf->pEnd;

	debug_print(WARNING, "%s(): buf->data: %p, (start:%d, end: %d), pid %d\n", __func__, buf->data, buf->pStart, buf->pEnd, getpid());
}

static void read_buf_head_room(plcBuffer *buf)
{
	int *data_info = (int *)((char *)buf->data - 2 * sizeof(int));

	buf->pStart = data_info[0];
	buf->pEnd = data_info[1];

	debug_print(WARNING, "%s() : buf->data: %p, (start:%d, end: %d), pid %d\n", __func__, buf->data, buf->pStart, buf->pEnd, getpid());
}
#endif

#ifdef USE_SHM
#if !defined(USE_SEM) && !defined(USE_SPIN)
static char rx_oct;
#endif
#endif

/*
 *  Read data from the socket
 */
static ssize_t plcSocketRecv(plcConn *conn, void *ptr, size_t len) {
#ifdef USE_SHM
    ssize_t sz;
    plcBuffer *buf = conn->buffer[PLC_INPUT_BUFFER];

	debug_print(WARNING, "\n%s() begins: pid %d\n", __func__, getpid());

	/* avoid warning. */
	((void)ptr); ((void)len);

#ifdef USE_SEM
	sem_t *sem = (sem_t *) ((char *) buf->data - PLC_BUFFER_HEADROOM);

	debug_print(WARNING, "  sem: %p", sem);
	do {
		sz = sem_wait(sem);
	} while (sz < 0 && (errno == EAGAIN || errno == EINTR));
#elif defined(USE_SPIN)
	volatile int *p = (int *) ((char *) buf->data - PLC_BUFFER_HEADROOM);

	debug_print(WARNING, "  spin receiving: %p", p);
	while (*p == 0); *p = 0;
	sz = 0;

#else
	do {
        sz = recv(conn->sock, &rx_oct, 1, 0);
	} while (sz < 0 && (errno == EAGAIN || errno == EINTR));
#endif

	if (sz < 0)
        lprintf(ERROR, "%s(), errno: %d", __func__, errno);

	read_buf_head_room(buf);

	debug_print(WARNING, "%s() ends: (return: %d, errno: %d), pid %d\n", __func__, (int) sz, errno, getpid());

	/* ret value means nothing for us. */
	return 1;
#else
    ssize_t sz = 0;

    while (sz <= 0) {
        sz = recv(conn->sock, ptr, len, 0);

        /* If receive command is terminated by SIGINT */
        if (sz < 0 && errno == EINTR) {
            lprintf(ERROR, "Query and PL/Container connections are terminated by user request");
        }

        /* If the command is terminated by another reason - standard handler */
        if ( !(sz == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) ) {
            break;
        }
    }

    return sz;
#endif
}

#ifdef USE_SHM
#ifndef USE_SEM
static const char tx_oct = '0';
#endif
#endif

/*
 *  Write data to the socket
 */
static ssize_t plcSocketSend(plcConn *conn, const void *ptr, size_t len) {
#ifdef USE_SHM
    ssize_t sz;
    plcBuffer *buf = conn->buffer[PLC_OUTPUT_BUFFER];

	debug_print(WARNING, "\n%s() begins: pid %d\n", __func__, getpid());

	write_buf_head_room(buf);

	((void)ptr);

#ifdef USE_SEM
	sem_t *sem = (sem_t *) ((char *) buf->data - PLC_BUFFER_HEADROOM);

	debug_print(WARNING, "  sem: %p", sem);

	do {
		sz = sem_post(sem);
	} while (sz < 0 && errno == EINTR);
#elif defined(USE_SPIN)
	int *p = (int *) ((char *) buf->data - PLC_BUFFER_HEADROOM);

	debug_print(WARNING, "  sending: %p", p);
	*p = 1;
	sz = 0;
#else
    do {
		sz = send(conn->sock, &tx_oct, 1, 0);
	} while (sz < 0 && errno == EINTR);
#endif

	debug_print(WARNING, "%s() ends: (return: %d, errno: %d), pid %d\n", __func__, (int) sz, errno, getpid());

	if (sz < 0)
        lprintf(ERROR, "%s(), errno: %d", __func__, errno);

	return len;

#else
    ssize_t sz = send(conn->sock, ptr, len, 0);

    /* If receive command is terminated by SIGINT */
    if (sz < 0 && errno == EINTR) {
        lprintf(ERROR, "Query and PL/Container connections are terminated by user request");
    }

    return sz;
#endif
}

/*
 * Function flushes the output buffer if it has reached a certain margin in
 * size or if the isForse parameter has passed to it
 *
 * Returns 0 on success, -1 on failure
 */
static int plcBufferMaybeFlush (plcConn *conn, bool isForse) {
#ifndef USE_SHM
    int res = 0;
#endif
    plcBuffer *buf = conn->buffer[PLC_OUTPUT_BUFFER];

    /*
     * Flush the buffer if it has less than PLC_BUFFER_MIN_FREE of free space
     * available or data size in the buffer is greater than initial buffer size
     * or if we are forced to flush everything
     */
    if (buf->bufSize - buf->pEnd < PLC_BUFFER_MIN_FREE
            || buf->pEnd - buf->pStart > PLC_BUFFER_SIZE
            || isForse) {
        // Flushing the data into channel
        while (buf->pStart < buf->pEnd) {
            int sent = 0;

            sent = plcSocketSend(conn,
                                 buf->data + buf->pStart,
                                 buf->pEnd - buf->pStart);
            if (sent <= 0) {
                lprintf(LOG, "plcBufferMaybeFlush: Socket write failed, send "
                             "return code is %d, error message is '%s'",
                             sent, strerror(errno));
                return -1;
            }
            buf->pStart += sent;
        }

#ifndef USE_SHM
		// Do not reset since the rx side is probably consuming the data.

        // After the flush we should consider resetting the buffer
        res = plcBufferMaybeReset(conn, PLC_OUTPUT_BUFFER);
        if (res < 0)
            return res;
#endif
    }

    return 0;
}

/*
 * Function resets the buffer from the current position to the beginning of
 * the buffer array if it has reached the middle of the buffer or the buffer
 * is empty
 *
 * Returns 0 on success, -1 on failure
 */
static int plcBufferMaybeReset (plcConn *conn, int bufType) {
    plcBuffer *buf = conn->buffer[bufType];

    // If the buffer has no data we can reset both pointers to 0
    if (buf->pStart == buf->pEnd) {
        buf->pStart = 0;
        buf->pEnd = 0;
#ifdef USE_SHM
		write_buf_head_room(buf);
#endif
    }

    /*
     * If our start point in a buffer has passed half of its size, we need
     * to move the data to the start of the buffer
     */
    if (buf->pStart > buf->bufSize / 2) {
        memcpy(buf->data, buf->data + buf->pStart, buf->pEnd - buf->pStart);
        buf->pEnd = buf->pEnd - buf->pStart;
        buf->pStart = 0;
#ifdef USE_SHM
		write_buf_head_room(buf);
#endif
    }


    return 0;
}

/*
 * Function checks whether we need to increase or decrease the size of the buffer.
 * Buffer grows if we want to insert more bytes than the amount of free space in
 * this buffer (given we leave PLC_BUFFER_MIN_FREE free bytes). Buffer shrinks
 * if we occupy less than 20% of its total space
 *
 * Returns 0 on success, -1 on failure
 */
static int plcBufferMaybeResize (plcConn *conn, int bufType, size_t bufAppend) {
    plcBuffer *buf = conn->buffer[bufType];
    int   dataSize;
    int   newSize;
    char *newBuffer = NULL;
    int   isReallocated = 0;

    // Minimum buffer size required to hold the data
    dataSize = (buf->pEnd - buf->pStart) + (int)bufAppend + PLC_BUFFER_MIN_FREE;

    // If the amount of data buffer currently holds and plan to hold after the
    // next insert is less than 20% of the buffer size, and if we have
    // previously increased the buffer size, we shrink it
    if (dataSize < buf->bufSize / 5 && buf->bufSize > PLC_BUFFER_SIZE) {
        // Buffer size is twice as large as the data we need to hold, rounded
        // to the nearest PLC_BUFFER_SIZE bytes
        newSize = ((dataSize * 2) / PLC_BUFFER_SIZE + 1) * PLC_BUFFER_SIZE;
#ifdef USE_SHM
        newBuffer = NULL;
#else
        newBuffer = (char*)plc_top_alloc(newSize);
#endif
        if (newBuffer == NULL) {
            lprintf(ERROR, "plcBufferMaybeFlush: Cannot allocate %d bytes "
                           "for output buffer", newSize);
            return -1;
        }
        isReallocated = 1;
    }

    // If we don't have enough space in buffer to handle the amount of data we
    // want to put there - we should increase its size
    else if (buf->pEnd + (int)bufAppend > buf->bufSize - PLC_BUFFER_MIN_FREE) {
        // Growing the buffer we need to just hold all the data we receive
        newSize = (dataSize / PLC_BUFFER_SIZE + 1) * PLC_BUFFER_SIZE;
#ifdef USE_SHM
        newBuffer = NULL;
#else
        newBuffer = (char*)plc_top_alloc(newSize);
#endif
        if (newBuffer == NULL) {
            lprintf(ERROR, "plcBufferMaybeGrow: Cannot allocate %d bytes for buffer",
                           newSize);
            return -1;
        }
        isReallocated = 1;
    }

    // If we have reallocated the buffer - copy the data over and free the old one
    if (isReallocated) {
        memcpy(newBuffer,
               buf->data + buf->pStart,
               (size_t)(buf->pEnd - buf->pStart));
        pfree(buf->data);
        buf->data = newBuffer;
        buf->pEnd = buf->pEnd - buf->pStart;
        buf->pStart = 0;
        buf->bufSize = newSize;
    }

    return 0;
}

/*
 * Append some data to the buffer. This function does not guarantee that the
 * data would be immediately sent, you have to forcefully flush buffer to
 * achieve this
 *
 * Returns 0 on success, -1 if failed
 */
int plcBufferAppend (plcConn *conn, char *srcBuffer, size_t nBytes) {
    int res = 0;
    plcBuffer *buf = conn->buffer[PLC_OUTPUT_BUFFER];

    // If we don't have enough space in the buffer to hold the data
    if (buf->bufSize - buf->pEnd < (int)nBytes) {

        // First thing to check - whether we can reset the data to the beginning
        // of the buffer, freeing up some space in the end of it
        res = plcBufferMaybeReset(conn, PLC_OUTPUT_BUFFER);
        if (res < 0)
            return res;

        // Second check - whether we need to flush the buffer as it holds much data
        res = plcBufferMaybeFlush(conn, false);
        if (res < 0)
            return res;

        // Third check - whether we need to resize our buffer after these manipulations
        res = plcBufferMaybeResize(conn,
                                   PLC_OUTPUT_BUFFER,
                                   nBytes);
        if (res < 0)
            return res;
    }

    // Appending data to the buffer
    memcpy(buf->data + buf->pEnd, srcBuffer, nBytes);
    buf->pEnd = buf->pEnd + nBytes;
    assert(buf->pEnd <= buf->bufSize);
    return 0;
}

/*
 * Read some data from the buffer. If buffer does not have enough data in it,
 * it will ask the socket to receive more data and put it into the buffer
 *
 * Returns 0 on success, -1 or -2 if failed
 */
int plcBufferRead (plcConn *conn, char *resBuffer, size_t nBytes) {
    plcBuffer *buf = conn->buffer[PLC_INPUT_BUFFER];
    int res = 0;

    res = plcBufferReceive (conn, nBytes);
    if (res == 0) {
        memcpy(resBuffer, buf->data + buf->pStart, nBytes);
        buf->pStart = buf->pStart + nBytes;
    } else {
        lprintf(LOG, "plcBufferRead: Socket read failed, "
                     "received return code is %d, error message is '%s'",
                     res, strerror(errno));
    }

    return res;
}

/*
 * Function checks whether we have nBytes bytes in the buffer. If not, it reads
 * the data from the socket. If the buffer is too small, it would be grown
 *
 * Returns 0 on success, -1 if failed and -2 if socket read has returned no data
 */
int plcBufferReceive (plcConn *conn, size_t nBytes) {
    int res = 0;
    plcBuffer *buf = conn->buffer[PLC_INPUT_BUFFER];

    // If we don't have enough data in the buffer already
    if (buf->pEnd - buf->pStart < (int)nBytes) {
#ifdef USE_SHM
		res = plcSocketRecv(conn, NULL, 0);
		((void)res);
#else
        int nBytesToReceive;
        int recBytes;

        // First thing to consider - resetting the data in buffer to the beginning
        // freeing up the space in the end to receive the data
        res = plcBufferMaybeReset(conn, PLC_INPUT_BUFFER);
        if (res < 0)
            return res;

        // Second step - check whether we really need to resize the buffer after this
        res = plcBufferMaybeResize(conn, PLC_INPUT_BUFFER, nBytes);
        if (res < 0)
            return res;

        // When we sure we have enough space - receive the related data
        nBytesToReceive = (int)nBytes - (buf->pEnd - buf->pStart);
        while (nBytesToReceive > 0) {
            recBytes = plcSocketRecv(conn,
                                     buf->data + buf->pEnd,
                                     buf->bufSize - buf->pEnd);
            if (recBytes <= 0) {
                return -1;
            }
            buf->pEnd += recBytes;
            nBytesToReceive -= recBytes;
            assert(buf->pEnd <= buf->bufSize);
        }
#endif
    }

    return 0;
}

/*
 * Function forcefully flushes the buffer
 *
 * Returns 0 on success, -1 if failed
 */
int plcBufferFlush (plcConn *conn) {
    return plcBufferMaybeFlush(conn, true);
}

#ifdef USE_SHM
static char *
plc_shmset(size_t bytes, char *fn, int proj_id, int *id)
{
	key_t key;
	int shmid;
	void *p;

	if ((key = ftok(fn, proj_id)) < 0) {
		lprintf(ERROR, "ftok fails: %d\n", errno);
		return NULL;
	}

	if ((shmid = shmget(key, bytes, 0666|IPC_CREAT|IPC_EXCL)) < 0) {
		if (errno == EEXIST) {
			shmid = shmget(key, bytes, 0666);
			p = shmat(shmid, NULL, 0);
		} else {
			lprintf(ERROR, "shmget fails (%d %s %d): %d\n", (int) bytes, fn, proj_id, errno);
			p = NULL;
		}
	} else {
		debug_print(WARNING, "shm create done by pid: %d\n", getpid());
		p = shmat(shmid, NULL, 0);
#ifdef USE_SEM
		/* sem is at the beginning of the buffer. */
		if (sem_init(p, 1, 0) != 0)
			lprintf(ERROR, "sem_init() fails with errno %d", errno);
		debug_print(WARNING, "sem_init at %p", p);
#endif
#ifdef USE_SPIN
		*((int *)p) = 0;
#endif
	}

	*id = shmid;

	debug_print(WARNING, "Use shm id %d, addr %p, pid: %d\n", *id, p, getpid());

	return p;
}

/* Set shared memory and semaphore in advance in QE. */
static void *shm_in, *shm_out;
static int shm_in_id, shm_out_id;

void
prepare_shm()
{
    shm_in = plc_shmset(PLC_BUFFER_SIZE + PLC_BUFFER_HEADROOM, "/home/gpadmin/share/out.shm", 'o', &shm_in_id);

    shm_out = plc_shmset(PLC_BUFFER_SIZE + PLC_BUFFER_HEADROOM, "/home/gpadmin/share/in.shm", 'i', &shm_out_id);
}
#endif

/*
 *  Initialize plcConn data structure and input/output buffers
 */
plcConn * plcConnInit(int sock) {
    plcConn *conn;

#ifdef USE_SHM
#ifdef COMM_STANDALONE
	int id;
#endif
#endif

    // Initializing main structures
    conn = (plcConn*)plc_top_alloc(sizeof(plcConn));
    conn->buffer[PLC_INPUT_BUFFER]  = (plcBuffer*)plc_top_alloc(sizeof(plcBuffer));
    conn->buffer[PLC_OUTPUT_BUFFER] = (plcBuffer*)plc_top_alloc(sizeof(plcBuffer));

    // Initializing buffers
#ifdef USE_SHM

#ifdef COMM_STANDALONE
	/* container */
    conn->buffer[PLC_INPUT_BUFFER]->data = plc_shmset(PLC_BUFFER_SIZE + PLC_BUFFER_HEADROOM, "/opt/share/in.shm", 'i', &id) + PLC_BUFFER_HEADROOM;
    conn->buffer[PLC_INPUT_BUFFER]->shmid = id;
#else
    conn->buffer[PLC_INPUT_BUFFER]->data = shm_in + PLC_BUFFER_HEADROOM;
    conn->buffer[PLC_INPUT_BUFFER]->shmid = shm_in_id;
#endif

#else
    conn->buffer[PLC_INPUT_BUFFER]->data = (char*)plc_top_alloc(PLC_BUFFER_SIZE);

#endif

    conn->buffer[PLC_INPUT_BUFFER]->bufSize = PLC_BUFFER_SIZE;
    conn->buffer[PLC_INPUT_BUFFER]->pStart = 0;
    conn->buffer[PLC_INPUT_BUFFER]->pEnd = 0;
#ifdef USE_SHM

#ifdef COMM_STANDALONE
    conn->buffer[PLC_OUTPUT_BUFFER]->data = plc_shmset(PLC_BUFFER_SIZE + PLC_BUFFER_HEADROOM, "/opt/share/out.shm", 'o', &id) + PLC_BUFFER_HEADROOM;
    conn->buffer[PLC_OUTPUT_BUFFER]->shmid = id;
#else
    conn->buffer[PLC_OUTPUT_BUFFER]->data = shm_out + PLC_BUFFER_HEADROOM;
    conn->buffer[PLC_OUTPUT_BUFFER]->shmid = shm_out_id;
#endif

#else
    conn->buffer[PLC_OUTPUT_BUFFER]->data = (char*)plc_top_alloc(PLC_BUFFER_SIZE);
#endif

    conn->buffer[PLC_OUTPUT_BUFFER]->bufSize = PLC_BUFFER_SIZE;
    conn->buffer[PLC_OUTPUT_BUFFER]->pStart = 0;
    conn->buffer[PLC_OUTPUT_BUFFER]->pEnd = 0;

    // Initializing control parameters
    conn->sock = sock;

    return conn;
}

/*
 *  Connect to the specified host of the localhost and initialize the plcConn
 *  data structure
 */
plcConn *plcConnect(int port) {
    struct sockaddr_un  raddr; /** Remote address */
    plcConn            *result = NULL;
    struct timeval      tv;
    int port1 = port;

    port1++;
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        lprintf(ERROR, "PLContainer: Cannot create socket");
        return result;
    }

    memset(&raddr, 0, sizeof(raddr));

    raddr.sun_family = AF_UNIX;
    strcpy(raddr.sun_path, "/home/gpadmin/share/server.socket");

    if (connect(sock, (const struct sockaddr *)&raddr,
            sizeof(raddr)) < 0) {
        lprintf(DEBUG1, "PLContainer: Failed to connect");
        return result;
    }

    /* Set socker receive timeout to 500ms */
    tv.tv_sec  = 0;
    tv.tv_usec = 500000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    result = plcConnInit(sock);

    return result;
}

/*
 *  Close the plcConn connection and deallocate the buffers
 */
void plcDisconnect(plcConn *conn) {
    if (conn != NULL) {
        close(conn->sock);
#ifdef USE_SHM
		shmdt(conn->buffer[PLC_INPUT_BUFFER]->data - PLC_BUFFER_HEADROOM);
		shmctl(conn->buffer[PLC_INPUT_BUFFER]->shmid, IPC_RMID, NULL);
		shmdt(conn->buffer[PLC_OUTPUT_BUFFER]->data - PLC_BUFFER_HEADROOM);
		shmctl(conn->buffer[PLC_OUTPUT_BUFFER]->shmid, IPC_RMID, NULL);
#else
        pfree(conn->buffer[PLC_INPUT_BUFFER]->data);
        pfree(conn->buffer[PLC_OUTPUT_BUFFER]->data);
#endif
        pfree(conn->buffer[PLC_INPUT_BUFFER]);
        pfree(conn->buffer[PLC_OUTPUT_BUFFER]);
        pfree(conn);
    }
    return;
}
