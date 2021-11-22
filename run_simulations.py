'''
This script runs the streaming simulation with a set of different parameters, including
the transport algorithm, link bit rate, adaptation algorithm, and pacing setting.

All results, including the recorded DASH log files, are saved in one directory tree.
'''

import subprocess
import os
import shutil
import sys
from pathlib import Path

bitrates = ["100Kbps", "500Kbps", "1Mbps", "2Mbps", "3Mbps", "4Mbps","5Mbps", "10Mbps", "100Mbps", "500Mbps", "1Gbps"]
algos = ["panda", "festive", "tobasco"]
pacingVals = ["true", "false"]
protocols = ["QUIC", "TCP"]

def getWafCommand(protocol, bitrate, algo, pacing, simId):
    args = [
        '--simulationId={}'.format(simId), 
        '--adaptationAlgo={}'.format(algo), 
        '--dataRate={}'.format(bitrate), 
        '--pacingEnabled={}'.format(pacing), 
        '--transportProtocol={}'.format(protocol)
    ]

    argsStr = " ".join(args)
   
    return './waf --run="dash-streaming {}"'.format(argsStr)

def getPacingString(pacing):
    return "pacing" if pacing == "true" else "no-pacing"

def getOutputFileName(protocol, bitrate, algo, pacing):
    pacingStr = getPacingString(pacing)
    return "output_{}_{}_{}_{}.log".format(protocol, algo, bitrate, pacingStr)

def getOutputDir(baseDir, protocol, bitrate, algo, pacing):
    pacingStr = getPacingString(pacing)
    return '{}/{}/{}/{}/{}/'.format(baseDir, protocol, algo, bitrate, pacingStr) # QUIC/festive/1Mbps/pacing for example

def copyFiles(srcDir, destDir):
    srcFiles = os.listdir(srcDir)
    for file_name in srcFiles:
        full_file_name = os.path.join(srcDir, file_name)
        if os.path.isfile(full_file_name):
            shutil.copy(full_file_name, destDir)

def getConfigString(protocol, bitrate, algo, pacing, simId):
    pacingStr = "pacing" if pacing == "true" else "no pacing"
    return 'Simulation {}: {} over {} @ {} - {}'.format(simId, algo, protocol, bitrate, pacingStr)

def runSimulation(baseDir, protocol, bitrate, algo, pacing, simId):
    print (getConfigString(protocol, bitrate, algo, pacing, simId))

    # Ensure that the output directory is created   
    outputDir = getOutputDir(baseDir, protocol, bitrate, algo, pacing)
    Path(outputDir).mkdir(parents=True, exist_ok=True)

    # Run the simulation
    print ("Running...")
    outputPath = outputDir + getOutputFileName(protocol, bitrate, algo, pacing)
    command = getWafCommand(protocol, bitrate, algo, pacing, simId)
    with open(outputPath, "w") as outputFile:
        subprocess.run(command, shell=True, stdout=outputFile, stderr=subprocess.STDOUT)
    
    # Copy the logs into the output directory
    logFilesDir = "dash-log-files/{}/{}/".format(algo, simId)
    copyFiles(logFilesDir, outputDir)

    print ("Done simulation\n")

def generateConfigs():
    id = 0
    for proto in protocols:
        for algo in algos:
            for bitrate in bitrates:
                for pacingVal in pacingVals:
                    yield [proto, bitrate, algo, pacingVal, id]
                    id = id + 1

def main(baseDir):
    for (protocol, rate, algo, pacing, id) in generateConfigs():
        runSimulation(baseDir, protocol, rate, algo, pacing, id)

if (__name__ == '__main__'):
    if len(sys.argv) != 2:
        print ("ERROR: Invalid number of arguments. Provide a single argument for the output directory.")
        exit(1)
    
    baseDir = sys.argv[1]
    print ("Saving all log files under {}\n".format(baseDir))
    main(baseDir)
