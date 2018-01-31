# this script should be run under the home directory of ns3
BASE_NUM=0
for k in {0..15}
do
  for i in {1..2}
  do
    DIR_NUM=$(($BASE_NUM + ($k * 2) + $i))
    mkdir -p run_multilayer_codedpkt/run_$DIR_NUM
    taskset -c $(($i+3)) ./waf --cwd=run_multilayer_codedpkt/run_$DIR_NUM --run "scratch/rpc_probe_new --ReconstructionPeriod=0.00025 --EnablePcapTracing=0 --EnableServerPcapTracing=0 --EnableTorPcapTracing=0 --EnableL2PcapTracing=0 --EnableL3PcapTracing=0 --SimulationStopTime=0.1 --NumDstServer=20" &
    sleep 6 # for rpc.cc to have different seeds
  done
  wait
done
