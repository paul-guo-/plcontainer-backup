package org.postgresql.plj;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

/**
 * Hello world!
 *
 */
public class Main
{
    public static void main( String[] args )
    {
      ByteBuffer byteBuffer = ByteBuffer.allocate(1024);
      try {
        ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
        serverSocketChannel.bind( new InetSocketAddress(8000) );
        while (true) {
          SocketChannel channel = serverSocketChannel.accept();

          int bytesRead;

          while ((bytesRead = channel.read(byteBuffer)) != -1){

            System.out.println( "Bytes Read: " + bytesRead );
            byteBuffer.flip();

            while (byteBuffer.hasRemaining()) {
              System.out.print((char)byteBuffer.get());
            }

            byteBuffer.clear();

          }
        }
      } catch (IOException e) {
        e.printStackTrace();
      }
    }
}
