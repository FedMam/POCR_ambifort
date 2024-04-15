import subprocess

test_file = './tests/mytest.vfg'
outfile_path = './cfl_edges.txt'
f = open(test_file, 'rb').read()
f_out = open(outfile_path, 'wb')
f_out.write(b'n1,n2,label\n')
for l in f.split(b'\n'):
	l2 = l.replace(b'\t',b',').replace(b' ',b',')
	if l2.find(b'_i') != -1:
		#replace i
		arr = l2.split(b',')
		num = arr.pop(3)
		l2 = arr[0]+b','+arr[1]+b','+arr[2].replace(b'_i', b'_'+num)
		f_out.write(l2+b'\n')
	else:
		f_out.write(l2+b'\n')		

'''
out = subprocess.check_output(['./Debug-build/bin/vf', '-std', test_file])
for l in out.split(b'\n'):
	if l and l.find(b'#') == -1 and l.find(b'AnalysisTime') == -1:
		l2 = l.replace(b'\t',b',').replace(b' ',b',')
		f_out.write(l2+b'\n')
'''
f_out.close()

'''

LOAD CSV WITH HEADERS FROM 'file:///cfl_edges.txt' AS row
MERGE (source:Node {id: toInteger(row.n1)})
MERGE (target:Node {id: toInteger(row.n2)})
MERGE (source)-[:EDGE_LABEL {label: row.label}]->(target)    


MATCH (n1)-[e:EDGE_LABEL { label: "A"}]->(n2) where n1.id = 1
RETURN n2.id

'''
