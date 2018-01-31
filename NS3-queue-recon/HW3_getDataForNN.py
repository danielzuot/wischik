#!/usr/bin/env python
import numpy
import sys
import random
import time
from scipy import sparse
from scipy import linalg
import argparse
import matplotlib
matplotlib.use('Agg')
from pylab import *

parser = argparse.ArgumentParser()
parser.add_argument('--rack_size', type=int, default=4)
parser.add_argument('--rack_num', type=int, default=4)
parser.add_argument('--l2_size', type=int, default=4)
parser.add_argument('--l2_num', type=int, default=1)
parser.add_argument('--l3_size', type=int, default=0)
parser.add_argument('-dir', '--rundir', type=str, default='.')
args = parser.parse_args()

rack_size = args.rack_size
rack_num = args.rack_num
l2_size = args.l2_size
l2_num = args.l2_num
l3_size = args.l3_size

simulation_stop_time = 20
time_resolution = 0.01
data_rate = 1000000000 #1Gbits/sec

queue_total_num = rack_num*rack_size*l2_num + 2 * l2_size*rack_num*l2_num + 2 * l3_size*(l2_size*l2_num)
print 'Total # of queues:', queue_total_num

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
#RTTinfo.sort(lambda x,y: int((1000000000*x[0]-1000000000*y[0])))

print len(RTTinfo), 'RTT info totally.'


#################################################################
#Build the probe list
#################################################################
line = 0

timeindex = 2
t = timeindex * time_resolution #Use the probes in t=2ms (1.5ms~2.5ms) to construct adjacency matrix

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

	probelist[probeKey] = probeIndex
	probeIndex += 1

	line = line + 1
	if line >= len(RTTinfo):
		break

print len(probelist)

#################################################################
#Build delay matrix
#################################################################

delayoutput = open('delay_vector.txt', 'w')
queueoutput = open('queue_vector.txt', 'w')

for runIndex in range(1,13):

	RTTinfo = [] 	#List of the round-trip packets
		#Each line: pkt sent time, src rack num, src server num in this rack, dst rack num, dst server num in this rack, selectIndex (which l2/l3-switch is used), RTT

	# Read RTT info of probe pkts
	fileinput = open('run_{}/RTTinfo_processed.txt'.format(runIndex))
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
	#RTTinfo.sort(lambda x,y: int((1000000000*x[0]-1000000000*y[0])))

	print len(RTTinfo), 'RTT info totally.'

	line = 0

	for timeindex in range(1,int(simulation_stop_time/time_resolution)):
		t = timeindex * time_resolution #The sampling time to estimate queue lengths
		#print 'time = '+str(t)

		if timeindex == 1:
			#Skip the t=0.01 data, because the paths taken by probes at this interval are different from other intervals
			continue

		#Initialize RTT
		RTT = numpy.zeros([ len(probelist) ])
		RTT_numProbes = numpy.zeros([ len(probelist) ])

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
			RTT[probelist[probeKey]] += RTTinfo[line][11]
			RTT_numProbes[probelist[probeKey]] += 1

			line = line + 1
			if line >= len(RTTinfo):
				break

		#Average probes in the same path
		for index in range(0, len(probelist)):
			if RTT_numProbes[index] != 0:
				RTT[index] = RTT[index] / RTT_numProbes[index]

		for index in range(len(RTT)):
			delayoutput.write('{} '.format(RTT[index]))
		delayoutput.write('\n')

	simQueue = numpy.loadtxt('run_{}/SimulatedQueue.txt'.format(runIndex))
	print len(simQueue[:,0]), len(simQueue[0,:])
	for index in range(1,int(simulation_stop_time/time_resolution)-1): #Skip the t=0.01 data, because the paths taken by probes at this interval are different from other intervals
		for queueIndex in range(queue_total_num):
			queueoutput.write('{} '.format(simQueue[queueIndex,index]))
		queueoutput.write('\n')


delayoutput.close()
queueoutput.close()