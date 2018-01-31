#!/usr/bin/env python
import numpy
import sys
import matplotlib
matplotlib.use('Agg')
from pylab import *

rack_size = int(sys.argv[1])
rack_num = int(sys.argv[2])
l2_size = int(sys.argv[3])
l2_num = int(sys.argv[4])
l3_size = int(sys.argv[5])

data_rate = 1000000000
link_delay = 1e-6

# Read estimated queue length info
estQueue = numpy.loadtxt('run_1/EstQueue.txt')

# Read simulated queue length info
simQueue = numpy.loadtxt('run_1/SimulatedQueue.txt')

for i in range(0, simQueue.shape[0]):
	figure()
	fill_between(estQueue[0], estQueue[i+1], 0, color='red', alpha=0.8, label='by RTT')
	plot(estQueue[0], simQueue[i], color='blue', linewidth=2, label='NS3')
	legend()
	if i < (rack_size * rack_num * l2_num): #Server-ToR queue pair
		title('Server '+str(i)+' <-- ToR '+str(i//rack_size) )

	elif i < (rack_size * rack_num * l2_num + l2_size * rack_num * l2_num): #ToR -> L2 Switch queue
		l2 = (i - (rack_size * rack_num * l2_num))//rack_num
		rack = i - (rack_size * rack_num * l2_num) - rack_num * l2 + (l2//l2_size)*rack_num
		title('ToR '+str(rack)+' --> L2-switch '+str(l2) )
	elif i < (rack_size * rack_num * l2_num + 2 * l2_size * rack_num * l2_num): #L2 Switch -> ToR queue
		l2 = (i - (rack_size * rack_num * l2_num + l2_size * rack_num * l2_num))//rack_num
		rack = i - (rack_size * rack_num * l2_num + l2_size * rack_num * l2_num) - rack_num * l2 + (l2//l2_size)*rack_num
		title('ToR '+str(rack)+' <-- L2-switch '+str(l2) )
	elif i < (rack_size * rack_num * l2_num + 2 * l2_size * rack_num * l2_num + l3_size*(l2_size*l2_num)): #L2 Switch -> L3 switch queue
		l3 = (i - (rack_size * rack_num * l2_num + 2 * l2_size * rack_num * l2_num))//(l2_size*l2_num)
		l2 = i - (rack_size * rack_num * l2_num + 2 * l2_size * rack_num * l2_num) - (l2_size*l2_num) * l3
		title('L2-switch '+str(l2)+' --> L3-switch '+str(l3) )
	else: #L3 Switch -> L2 switch queue
		l3 = (i - (rack_size * rack_num * l2_num + 2 * l2_size * rack_num * l2_num + l3_size*(l2_size*l2_num)))//(l2_size*l2_num)
		l2 = i - (rack_size * rack_num * l2_num + 2 * l2_size * rack_num * l2_num + l3_size*(l2_size*l2_num)) - (l2_size*l2_num) * l3
		title('L2-switch '+str(l2)+' <-- L3-switch '+str(l3) )

	ylim([ 0, max(estQueue.max(), simQueue.max()) ])
	xlabel('Time (sec)')
	ylabel('Queue length (Bytes)')
	savefig('run_1/fig/fig'+str(i)+'.png')


RMSE = numpy.zeros([simQueue.shape[0],1])
RMS = numpy.zeros([simQueue.shape[0],1]) #Root of mean of squares
for i in range(0, simQueue.shape[0]):
	#if i < rack_size*rack_num: #Server-ToR queue pair
	#	linknum = 2
	#else: #ToR -> L2 Switch queue OR L2 Switch -> ToR queue
	linknum = 1
	RMSE[i,0] = sum(numpy.square(estQueue[i+1,:]-data_rate/8*link_delay*linknum-simQueue[i,:]))
	RMS[i,0] = sum(numpy.square(simQueue[i,:]))

total_RMSE = math.sqrt( numpy.sum( RMSE ) / (simQueue.shape[0]*simQueue.shape[1]) )
total_RMS = math.sqrt( numpy.sum( RMS ) / (simQueue.shape[0]*simQueue.shape[1]) )
total_percent = total_RMSE / total_RMS

print total_RMSE
print total_RMS
print total_percent
