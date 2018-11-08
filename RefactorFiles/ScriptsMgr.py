#import RefactorFiles as rf
import importlib.util


#--------------------------------------------------------------------------------------------------------------
def check_with_script(oldFileName, scriptName):
    module = importlib.import_module(scriptName)

    # reading from file
    lines = [];
    inputFile = open(oldFileName,'r', encoding='utf-8');
    for line in inputFile:
        lines.append(line);

    inputFile.close();

    return module.check_file(lines)

#--------------------------------------------------------------------------------------------------------------
def fix_with_script(newFileName, scriptName):
    module = importlib.import_module(scriptName)

    # reading from file
    lines = [];
    inputFile = open(newFileName,'r', encoding='utf-8');
    for line in inputFile:
        lines.append(line);
    inputFile.close();

    results = module.fix_file(lines)

     # writing into file
    outputFile = open(newFileName,'w', encoding='utf-8');
    outputFile.truncate(0);

    for result in results:
        outputFile.write(result)

    outputFile.close();

#--------------------------------------------------------------------------------------------------------------
def RewriteFromTo(srcFileName, dstFileName):

    # reading from file
    lines = [];
    inputFile = open(srcFileName,'r');
    for line in inputFile:
        lines.append(line);

    inputFile.close();


    # writing into file
    outputFile = open(dstFileName,'r+');
    outputFile.truncate(0);

    for result in lines:
        outputFile.write(result)

    outputFile.close();