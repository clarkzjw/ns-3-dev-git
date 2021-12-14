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

bitrates = ["500Kbps", "1Mbps", "2Mbps", "3Mbps", "5Mbps"]
algos = ["festive", "tobasco"]
pacingVals = [True, False]
protocols = ["QUIC", "TCP"]
errs = [0, 0.01, 0.02, 0.05]

class SimulationConfig:
    def __init__ (self, simId, protocol, algo, bitrate, err, pacing):
        self.simId = simId
        self.protocol = protocol
        self.algo = algo
        self.bitrate = bitrate
        self.err = err
        self.pacing = pacing       

def getWafCommand(config):
    args = [
        '--simulationId={}'.format(config.simId), 
        '--adaptationAlgo={}'.format(config.algo), 
        '--dataRate={}'.format(config.bitrate), 
        '--pacingEnabled={}'.format(config.pacing), 
        '--transportProtocol={}'.format(config.protocol),
        '--errorRate={}'.format(config.err)
    ]

    argsStr = " ".join(args)
   
    return './waf --run="dash-streaming {}"'.format(argsStr)

def getPacingString(pacing):
    if pacing:
        return "pacing"
    else:
        return "no-pacing"

def getOutputFileName(config):
    pacingStr = getPacingString(config.pacing)
    return "output_{}_{}_{}_{}.log".format(config.protocol, config.algo, config.bitrate, pacingStr)

def getOutputDir(baseDir, config):
    pacingStr = getPacingString(config.pacing)
    return '{}/{}/{}/{}/{}/{}/'.format(baseDir, config.protocol, config.algo, config.bitrate, config.err, pacingStr) # QUIC/festive/1Mbps/0.01/pacing for example

def copyFiles(srcDir, destDir):
    srcFiles = os.listdir(srcDir)
    for file_name in srcFiles:
        full_file_name = os.path.join(srcDir, file_name)
        if os.path.isfile(full_file_name):
            shutil.copy(full_file_name, destDir)

def getConfigString(config):
    pacingStr = getPacingString(config.pacing)
    errStr = str(config.err*100)
    return 'Simulation {}: {} over {} @ {} with {} percent error - {}'.format(config.simId, config.algo, config.protocol, config.bitrate, errStr, pacingStr)

def countSuccessfulDownloads(logDir):
    downloadLogFile = logDir + "cl0_downloadLog.txt"

    lastline = ''
    with open(downloadLogFile) as f:
        lastline = f.readlines()[-1]

    maxSegment = int(lastline.split()[0]) 
    return maxSegment + 1

def runSimulation(baseDir, config):
    print (getConfigString(config))

    # Ensure that the output directory is created   
    outputDir = getOutputDir(baseDir, config)
    Path(outputDir).mkdir(parents=True, exist_ok=True)
    print ('Created output dir: ', outputDir)

    # Run the simulation
    print ("Running...")
    outputPath = outputDir + getOutputFileName(config)
    command = getWafCommand(config)
    with open(outputPath, "w") as outputFile:
        subprocess.run(command, shell=True, stdout=outputFile, stderr=subprocess.STDOUT)
    
    # Copy the logs into the output directory
    logFilesDir = "dash-log-files/{}/{}/".format(config.algo, config.simId)
    copyFiles(logFilesDir, outputDir)

    # Count the number of segments that were successfully downloaded
    count = countSuccessfulDownloads(logFilesDir)
    print ("{} segments downloaded successfully".format(count))

    print ("Done simulation\n")


def generateConfigs(startingSimId = 0):
    id = startingSimId
    for proto in protocols:
        for algo in algos:
            for bitrate in bitrates:
                for err in errs:
                    for pacingVal in pacingVals:
                        yield SimulationConfig(id, proto, algo, bitrate, err, pacingVal)
                        id = id + 1

def main(baseDir, startingSimId):
    for config in generateConfigs(startingSimId):
        runSimulation(baseDir, config)

if (__name__ == '__main__'):
    if len(sys.argv) != 3:
        print ("Usage: <output dir> <starting simulation id>")
        exit(1)
    
    baseDir = sys.argv[1]
    startingId = int(sys.argv[2])
    print ("Saving all log files under {}\n".format(baseDir))
    main(baseDir, startingId)
