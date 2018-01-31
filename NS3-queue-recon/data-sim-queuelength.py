#!/usr/bin/env python
import numpy
import sys

rack_size = int(sys.argv[1])
rack_num = int(sys.argv[2])
l2_size = int(sys.argv[3])
l2_num = int(sys.argv[4])
l3_size = int(sys.argv[5])

simulation_stop_time = 0.05
time_resolution = 0.001

# Read queue length
q = numpy.loadtxt('rpcoutput.log')
print q.shape

for i in range(0, len(q[:,3])):
	q[i,3] = q[i,3] / float(1000000000)

queue_total_num = rack_num*rack_size*l2_num + 2 * l2_size*rack_num*l2_num + 2 * l3_size*(l2_size*l2_num)
print 'Total # of queues:', queue_total_num

#Each row: the queue length for each queue (pair), use the same timestamp as estQ_global
#Sequence: 	Server 0 <--> its ToR
#		Server 1 <--> its ToR
#		......
#		Server (rack_size*rack_num -1) <--> its ToR
#		l2 switch 0 <--- ToR 0
#		l2 switch 0 <--- ToR 1
#		......
#		l2 switch 0 <--- ToR (rack_num -1)
#		l2 switch 1 <--- ToR 0
#		l2 switch 1 <--- ToR 1
#		......
#		l2 switch 1 <--- ToR (rack_num -1)
#		......
#		......
#		......
#		l2 switch (l2_num-1) <--- ToR 0
#		l2 switch (l2_num-1) <--- ToR 1
#		......
#		l2 switch (l2_num-1) <--- ToR (rack_num -1)
#		l2 switch 0 ---> ToR 0
#		l2 switch 0 ---> ToR 1
#		......
#		l2 switch 0 ---> ToR (rack_num -1)
#		l2 switch 1 ---> ToR 0
#		l2 switch 1 ---> ToR 1
#		......
#		l2 switch 1 ---> ToR (rack_num -1)
#		......
#		......
#		......
#		l2 switch (l2_num-1) ---> ToR 0
#		l2 switch (l2_num-1) ---> ToR 1
#		......
#		l2 switch (l2_num-1) ---> ToR (rack_num -1)
qlen = numpy.zeros([ queue_total_num, simulation_stop_time/time_resolution-1 ])

#Keep track how many times this queue (pair) is sampled
qnum = numpy.zeros([ queue_total_num, simulation_stop_time/time_resolution-1 ])

line = 0

for i in range(0, int(simulation_stop_time/time_resolution)-1):
	t = (i+1) * time_resolution
#	print t
	while ( q[line,3] < (t-time_resolution/2) ):
		line = line + 1
	while ( q[line,3] < (t+time_resolution/2) ):
		if q[line,0] == 0: #Type 0: Server
			line = line + 1
			continue #Skip the queues in servers
		elif q[line,0] == 1: #Type 1: ToR
			if q[line,2] <= rack_size: #This link is connected to server
				index = q[line,1]*rack_size + q[line,2]-1
			else: #This link is connected to l2-switch
				l2block_index = int(round(q[line,1]))/rack_num
				index = rack_size*rack_num*l2_num + l2block_index*l2_size*rack_num + (q[line,2]-rack_size-1) * rack_num + q[line,1] - l2block_index*rack_num
		elif q[line,0] == 2: #Type 2: l2-switch
			if q[line,2] <= rack_num: #This link is connected to ToR
				index = rack_size*rack_num*l2_num + l2_size*rack_num*l2_num + q[line,1]*rack_num + q[line,2]-1
			else: #This link is connected to l3_server
				index = rack_size*rack_num*l2_num + 2*l2_size*rack_num*l2_num + (q[line,2]-rack_num-1) * l2_size*l2_num + q[line,1]
		else: #Type 3: l3-switch
			index = rack_size*rack_num*l2_num + 2*l2_size*rack_num*l2_num + l2_size*l2_num*l3_size + q[line,1]*l2_size*l2_num + q[line,2]-1

		qlen[ index, i ] = ( qlen[ index, i ] * qnum[ index, i ] + q[line,4] ) / ( qnum[ index, i ] + 1)
		qnum[ index, i ] += 1
		line = line + 1

#Output of the simulated queue lengths
fileoutput = open('SimulatedQueue.txt','w')
for j in range(0, queue_total_num):
	for k in range(0, int(simulation_stop_time/time_resolution)-1):
		fileoutput.write( str( qlen[j,k] ) + ' ' )
	fileoutput.write('\n')
fileoutput.close()
