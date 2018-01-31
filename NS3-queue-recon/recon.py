#!/usr/bin/env python
import numpy
from sklearn import datasets, linear_model
import sys
import random
import time
from scipy import sparse
from scipy import linalg

rack_size = int(sys.argv[1])
rack_num = int(sys.argv[2])
l2_size = int(sys.argv[3])
l2_num = int(sys.argv[4])
l3_size = int(sys.argv[5])

simulation_stop_time = 20
time_resolution = 0.01
data_rate = 1000000000 #1Gbits/sec

queue_total_num = rack_num*rack_size*l2_num + 2 * l2_size*rack_num*l2_num + 2 * l3_size*(l2_size*l2_num)
print 'Total # of queues:', queue_total_num

lasso_alpha = 1e-6/( 2 * ( queue_total_num  ) / 32.0)

batch_size = 2

server_num = rack_size * rack_num * l2_num
RTTinfo = [] 	#List of the round-trip packets
		#Each line: pkt sent time, src rack num, src server num in this rack, dst rack num, dst server num in this rack, selectIndex (which l2/l3-switch is used), RTT

# Read RTT info of probe pkts
fileinput = open('run_1/RTTinfo_processed.txt')
for linetext in fileinput:
	pktinfo = linetext.split(' ')
	pktinfo[0] = float( pktinfo[0] )
	pktinfo[len(pktinfo)-2] = float( pktinfo[len(pktinfo)-2] )

	for tmp in range(1,len(pktinfo)-2):
		pktinfo[tmp] = int(pktinfo[tmp])

	pktinfo.pop()

	RTTinfo.append(pktinfo)

fileinput.close()

#Sort RTT info by timestamp
RTTinfo.sort(lambda x,y: int((1000000000*x[0]-1000000000*y[0])))

print len(RTTinfo), 'RTT info totally.'


#The 0th row: the timestamp
#Each other row: the estimated queue length for each queue (pair)
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
estQ_global = numpy.zeros([1 + queue_total_num, int(simulation_stop_time/time_resolution)-1])

#sampling_time = numpy.zeros([1, queue_total_num])



#################################################################
#Estimate queue lengths
#################################################################
LassoTotalTime = 0

starttime = time.time()
print 'start time =', starttime


#################################################################
#Build the Adjacency matrix
#################################################################
line = 0

timeindex = 2
t = timeindex * time_resolution #Use the probes in t=2ms (1.5ms~2.5ms) to construct adjacency matrix

Astart = time.time()
#Initialize X
X = numpy.mat([])
probelist = {}

while (RTTinfo[line][0] < t-time_resolution/2):
	line = line + 1

probeIndex = 0
while (RTTinfo[line][0] < t+time_resolution/2): #This pkt is within the time range we are measuring
	probeKey = (RTTinfo[line][1], RTTinfo[line][2], RTTinfo[line][3], RTTinfo[line][4], RTTinfo[line][5], RTTinfo[line][6], RTTinfo[line][7]) #Use the src/dst server, and selectindex 0~2 (single-way selectindex) as the key of each probe

	if probeKey in probelist:
		line = line + 1
		if line >= len(RTTinfo):
			break
		continue

	link = numpy.zeros([1, queue_total_num]) #Generate the link that this pkt passes through
	src_l2block_index = RTTinfo[line][1]/rack_num
	dst_l2block_index = RTTinfo[line][3]/rack_num

	if RTTinfo[line][1] == RTTinfo[line][3]: #Source and destination are in the same rack
		link[ 0, RTTinfo[line][1] * rack_size + RTTinfo[line][2] ] += 1
		link[ 0, RTTinfo[line][3] * rack_size + RTTinfo[line][4] ] += 1

	elif src_l2block_index == dst_l2block_index: #Source and destination are in the same l2-block, but in different racks
		link[ 0, RTTinfo[line][1] * rack_size + RTTinfo[line][2] ] += 1
		link[ 0, RTTinfo[line][3] * rack_size + RTTinfo[line][4] ] += 1

		link[ 0, rack_size*rack_num*l2_num + src_l2block_index*l2_size*rack_num + RTTinfo[line][5] * rack_num + RTTinfo[line][1]-src_l2block_index*rack_num ] += 1
		link[ 0, rack_size*rack_num*l2_num + l2_size*rack_num*l2_num + dst_l2block_index*l2_size*rack_num + RTTinfo[line][5] * rack_num + RTTinfo[line][3]-dst_l2block_index*rack_num ] += 1
		link[ 0, rack_size*rack_num*l2_num + dst_l2block_index*l2_size*rack_num + RTTinfo[line][8] * rack_num + RTTinfo[line][3]-dst_l2block_index*rack_num ] += 1
		link[ 0, rack_size*rack_num*l2_num + l2_size*rack_num*l2_num + src_l2block_index*l2_size*rack_num + RTTinfo[line][8] * rack_num + RTTinfo[line][1]-src_l2block_index*rack_num ] += 1

	else: #Source and destination are in different l2-blocks
		link[ 0, RTTinfo[line][1] * rack_size + RTTinfo[line][2] ] += 1
		link[ 0, RTTinfo[line][3] * rack_size + RTTinfo[line][4] ] += 1

		link[ 0, rack_size*rack_num*l2_num + src_l2block_index*l2_size*rack_num + RTTinfo[line][5] * rack_num + RTTinfo[line][1]-src_l2block_index*rack_num ] += 1
		link[ 0, rack_size*rack_num*l2_num + 2*l2_size*rack_num*l2_num + RTTinfo[line][6]*l2_size*l2_num + src_l2block_index*l2_size+RTTinfo[line][5] ] += 1
		link[ 0, rack_size*rack_num*l2_num + 2*l2_size*rack_num*l2_num + l2_size*l2_num*l3_size + RTTinfo[line][6]*l2_size*l2_num + dst_l2block_index*l2_size+RTTinfo[line][7] ] += 1
		link[ 0, rack_size*rack_num*l2_num + l2_size*rack_num*l2_num + dst_l2block_index*l2_size*rack_num + RTTinfo[line][7] * rack_num + RTTinfo[line][3]-dst_l2block_index*rack_num ] += 1

		link[ 0, rack_size*rack_num*l2_num + dst_l2block_index*l2_size*rack_num + RTTinfo[line][8] * rack_num + RTTinfo[line][3]-dst_l2block_index*rack_num ] += 1
		link[ 0, rack_size*rack_num*l2_num + 2*l2_size*rack_num*l2_num + RTTinfo[line][9]*l2_size*l2_num + dst_l2block_index*l2_size+RTTinfo[line][8] ] += 1
		link[ 0, rack_size*rack_num*l2_num + 2*l2_size*rack_num*l2_num + l2_size*l2_num*l3_size + RTTinfo[line][9]*l2_size*l2_num + src_l2block_index*l2_size+RTTinfo[line][10] ] += 1
		link[ 0, rack_size*rack_num*l2_num + l2_size*rack_num*l2_num + src_l2block_index*l2_size*rack_num + RTTinfo[line][10] * rack_num + RTTinfo[line][1]-src_l2block_index*rack_num ] += 1


	if probeIndex == 0: #X is empty
		X = link
	else: #Append to X
		X = numpy.r_[X,link]

	probelist[probeKey] = probeIndex
	probeIndex += 1

	line = line + 1
	if line >= len(RTTinfo):
		break

X_sp = sparse.coo_matrix(X)

Aend = time.time()
print 'Time for building adjacency matrix', Aend-Astart


print 'Shape of adjacency matrix',X.shape
#print probelist.shape

sampling_time = numpy.sum(X,axis=0)
#Output of sampling time
fileoutput = open('run_1/SamplingTime.txt','w')
for j in range(0, queue_total_num):
	fileoutput.write( str( sampling_time[j] ) + '\n' )
fileoutput.close()
print 'SamplingTime has outputed!'

#################################################################
#Reconstruct queues
#################################################################
line = 0

for timeindex in range(1,int(simulation_stop_time/time_resolution)):
	t = timeindex * time_resolution #The sampling time to estimate queue lengths
	#print 'time = '+str(t)

	if timeindex == 1:
		#print 'Skip time '+str(t)
		estQ_global[0, timeindex-1] = t
		for j in range(0, queue_total_num):
			estQ_global[j+1, timeindex-1] = 0
		continue

	index_in_batch = (timeindex-2)%batch_size

	#Initialize RTT
	if index_in_batch == 0:
		RTT = numpy.zeros([ X.shape[0] , batch_size ])
		RTT_numProbes = numpy.zeros([ X.shape[0] , batch_size ])

	while (RTTinfo[line][0] < t-time_resolution/2):
		line = line + 1

	while (RTTinfo[line][0] < t+time_resolution/2): #This pkt is within the time range we are measuring
		probeKey = (RTTinfo[line][1], RTTinfo[line][2], RTTinfo[line][3], RTTinfo[line][4], RTTinfo[line][5], RTTinfo[line][6], RTTinfo[line][7])
		if probeKey not in probelist:
			#print 'This probe is not founded in adjacency matrix'
			line = line + 1
			if line >= len(RTTinfo):
				break
			continue
		RTT[probelist[probeKey],index_in_batch] += RTTinfo[line][11]
		RTT_numProbes[probelist[probeKey],index_in_batch] += 1

		line = line + 1
		if line >= len(RTTinfo):
			break

	#Average probes in the same path
	for index in range(0, X.shape[0]):
		if RTT_numProbes[index, index_in_batch] != 0:
			RTT[index, index_in_batch] = RTT[index, index_in_batch] / RTT_numProbes[index, index_in_batch]

	#Fill in the missed RTTs (due to drops of probe pkts)
	'''
	estimated_missedRTT = max(RTT[:,index_in_batch])
	for j in range(0,len(RTT[:,index_in_batch])):
		if RTT[j,index_in_batch] == 0:
			RTT[j,index_in_batch] = estimated_missedRTT
	'''


#	print X
#	print X.T.dot(X)
#	print len(RTT)

#	print numpy.linalg.det(X.T.dot(X))
#	print numpy.linalg.det(X.dot(X.T))

#	fileoutput = open('X.txt','w')
#	for j in range(0, numpy.size(RTT) ):
#		for k in range(0, int(numpy.size(X)/numpy.size(RTT)) ):
#			fileoutput.write( str( X[j,k] ) + ' ' )
#		fileoutput.write('\n')
#	fileoutput.close()

	if index_in_batch == batch_size-1:

		startLassotime = time.time()

		#Estimate the queue length
		regr = linear_model.Lasso(alpha=lasso_alpha, fit_intercept=False, positive=True)
		regr.fit(X_sp, RTT)
		endLassotime = time.time()
		estQ = regr.coef_ * data_rate / 8

		#	print X
		#	print numpy.size(X)
		#	print keepcolumn
		#	print estQ
		
		#Write estQ into estQ_global
		for k in range( 0, batch_size):
			estQ_global[0, timeindex-batch_size+k] = (timeindex-batch_size+k+1) * time_resolution
			for j in range(0, queue_total_num):
				estQ_global[j+1, timeindex-batch_size+k] = estQ[k,j]

		LassoTotalTime = LassoTotalTime + endLassotime - startLassotime

endtime = time.time()
print 'end time =', endtime
print 'Total used time =', endtime - starttime
print 'Lasso time =', LassoTotalTime

#Output of estimated queue lengths
fileoutput = open('run_1/EstQueue.txt','w')
for j in range(0, queue_total_num+1):
	for k in range(0, int(simulation_stop_time/time_resolution)-1):
		fileoutput.write( str( estQ_global[j,k] ) + ' ' )
	fileoutput.write('\n')
fileoutput.close()

