/* ========================== R Functions ========================== */

CREATE OR REPLACE FUNCTION rtestudt3(r test_type3[]) RETURNS varchar AS $$
# container: plc_r
x=''
for (row in r){
x=paste(x,'#', row[1,1],'|',row[1,2],'|', row[1,3])
}
return(x)

$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt4(r test_type4[]) RETURNS varchar AS $$
# container: plc_r
res = ''
for (row in r){
    res =paste(res,'#' ,row[1,1],'|',sum(row[,2]),'|',',',paste(row[1,3]))
}
return (res)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt5(r test_type4[]) RETURNS int AS $$
# container: plc_r
if (is.null(r)){
    return(1)
}
for (el in r){
    if (is.null(el)){
        return(2)
    }
}
return(3)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt7() RETURNS test_type3[] AS $$
# container: plc_r
a=1
b=2
c='foo'
a1=data.frame(a,b,c)
a=3
b=4
c='bar'
a2=data.frame(a,b,c)
v=list()
v[[1]]=a1
v[[2]]=a2
return (v)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt9() RETURNS SETOF test_type3[] AS $$
# container: plc_r
l1=list()
l2=list()
l3=list()

l1[[1]]=data.frame(1,2,'foo')
l1[[2]]=data.frame(3,4,'bar')

l2[[1]]=data.frame(5,6,'buz')
l2[[2]]=data.frame(7,8,'zzz')


l3[[1]]=l1
l3[[2]]=l2
return (l3)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt10() RETURNS test_type4[] AS $$
# container: plc_r
v=list()
v[[1]] = data.frame(1,c(2,22),c('foo','foo2'))
v[[2]] = data.frame(3,c(4,44),c('bar','bar2'))
return(v)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt12() RETURNS SETOF test_type4[] AS $$
# container: plc_r
return [ [{'a': 1, 'b': [2,22], 'c': ['foo','foo2']}, {'a': 3, 'b': [4,44], 'c': ['bar','bar2']}],
         [{'a': 5, 'b': [6,66], 'c': ['buz','buz2']}, {'a': 7, 'b': [8,88], 'c': ['zzz','zzz2']}] ]
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt14(r test_type3[]) RETURNS test_type3[] AS $$
# container: plc_r
return(r)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt15(r test_type3[]) RETURNS SETOF test_type3 AS $$
# container: plc_r
return(r)
$$ LANGUAGE plcontainer;

/* ========================== Python Functions ========================== */

CREATE OR REPLACE FUNCTION pytestudt3(r test_type3[]) RETURNS varchar AS $$
# container: plc_python
res = ''
for row in r:
    res += '#' + str(row['a']) + '|' + str(row['b']) + '|' + row['c']
return res
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt4(r test_type4[]) RETURNS varchar AS $$
# container: plc_python
res = ''
for row in r:
    res += '#' + str(row['a']) + '|' + str(sum(row['b'])) + '|' + ','.join(row['c'])
return res
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt5(r test_type4[]) RETURNS int AS $$
# container: plc_python
if r is None:
    return 1
for el in r:
    if el is None:
        return 2
return 3
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt7() RETURNS test_type3[] AS $$
# container: plc_python
return [{'a': 1, 'b': 2, 'c': 'foo'}, {'a': 3, 'b': 4, 'c': 'bar'}]
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt9() RETURNS SETOF test_type3[] AS $$
# container: plc_python
return [ [{'a': 1, 'b': 2, 'c': 'foo'}, {'a': 3, 'b': 4, 'c': 'bar'}],
         [{'a': 5, 'b': 6, 'c': 'buz'}, {'a': 7, 'b': 8, 'c': 'zzz'}] ]
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt10() RETURNS test_type4[] AS $$
# container: plc_python
return [{'a': 1, 'b': [2,22], 'c': ['foo','foo2']}, {'a': 3, 'b': [4,44], 'c': ['bar','bar2']}]
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt12() RETURNS SETOF test_type4[] AS $$
# container: plc_python
return [ [{'a': 1, 'b': [2,22], 'c': ['foo','foo2']}, {'a': 3, 'b': [4,44], 'c': ['bar','bar2']}],
         [{'a': 5, 'b': [6,66], 'c': ['buz','buz2']}, {'a': 7, 'b': [8,88], 'c': ['zzz','zzz2']}] ]
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt14(r test_type3[]) RETURNS test_type3[] AS $$
# container: plc_python
return r
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pytestudt15(r test_type3[]) RETURNS SETOF test_type3 AS $$
# container: plc_python
return r
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pybadudtarr() RETURNS test_type3[] AS $$
# container: plc_python
return [ {'a': 1, 'b': 2, 'c': 'foo'}, {'a': 1, 'b': 2} ]
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pybadudtarr2() RETURNS test_type4[] AS $$
# container: plc_python
return [ {'a': 1, 'b': [2, 3], 'c': ['foo', 'bar']},
         {'a': 4, 'b': [5, 'f'], 'c': ['a', 'b']} ]
$$ LANGUAGE plcontainer;