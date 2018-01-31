#!/usr/bin/env python
import numpy
import sys
import argparse
import gc

parser = argparse.ArgumentParser()
parser.add_argument('--rack_size', type=int, default=4)
parser.add_argument('--rack_num', type=int, default=4)
parser.add_argument('--l2_size', type=int, default=4)
parser.add_argument('--l2_num', type=int, default=4)
parser.add_argument('--l3_size', type=int, default=4)
parser.add_argument('-dir', '--rundir', type=str, default='.')
args = parser.parse_args()

rack_size = args.rack_size
rack_num = args.rack_num
l2_size = args.l2_size
l2_num = args.l2_num
l3_size = args.l3_size
protocol_TCP = 6
protocol_UDP = 17
UDP_port_echo = 10
simulation_stop_time = 0.05
time_resolution = 0.001

server_num = rack_size * rack_num * l2_num
RTTinfo = [] 	#List of the round-trip packets
		#Each line: pkt sent time, src rack num, src server num in this rack, dst rack num, dst server num in this rack, selectIndex (which l2-switch is used), RTT
fileoutput = open(args.rundir + '/RTTinfo_processed.txt','w')

fileinput = open(args.rundir + '/RTTinfo.txt')

#################################################################
# Convert pkt info strings to pkt parameters
#################################################################
for linetext in fileinput:
	pktinfo = linetext.split(' ')
	src_rack_index = int(pktinfo[0])
	src_server_index = int(pktinfo[1])
	dst_rack_index = int(pktinfo[2])
	dst_server_index = int(pktinfo[3])
	selectindex0 = int(pktinfo[4])
	selectindex1 = int(pktinfo[5])
	selectindex2 = int(pktinfo[6])
	selectindex3 = int(pktinfo[11])
	selectindex4 = int(pktinfo[12])
	selectindex5 = int(pktinfo[13])
	senttime = float(pktinfo[18])/1e9
	RTT = ( ( float(pktinfo[21][:-1]) - float(pktinfo[18]) ) - ( float(pktinfo[20]) - float(pktinfo[19]) ) )/1e9
	RTTinfo.append([ senttime, src_rack_index, src_server_index, dst_rack_index, dst_server_index, selectindex0, selectindex1, selectindex2, selectindex3, selectindex4, selectindex5, RTT ])

fileinput.close()

	
#Sort RTT info by timestamp
RTTinfo.sort(lambda x,y: int((1000000000*x[0]-1000000000*y[0])))

for line in range(0,len(RTTinfo)):
	for i in range(0,len(RTTinfo[0])):
		fileoutput.write( str(RTTinfo[line][i]) + ' ' )
	fileoutput.write('\n')

fileoutput.close()
