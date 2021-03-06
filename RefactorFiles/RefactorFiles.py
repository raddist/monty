#import fileinput

#with fileinput.FileInput('text.cpp', inplace=True, backup='.bak') as file:
# for line in file:
# print(line.rstrip())

import re

#--------------------------------------------------------------------------------------------------------------
def is_empty(container):
    if container:
        return True;
    else:
        return False;

#--------------------------------------------------------------------------------------------------------------
def GetComment(lines):
    results = [];
    results.append('\n');
    results.append('\n');
    results.append('//------------------------------------------------------------------------------\n');
    results.append('/*\n');
    if lines:
        results += lines;
    results.append('\n')
    results.append('*/\n');
    results.append('//---\n');
    return results;


#--------------------------------------------------------------------------------------------------------------
def ExtractComment(lines):
    results = [];
    for line in lines:
        sRes = re.findall(r'\w+.*', line);
        if sRes:
            results.append('  ' + sRes[0]);
    return results;

#--------------------------------------------------------------------------------------------------------------
def RefactorFile(fileName):

    # reading from file
    lines = [];
    inputFile = open(fileName,'r+');
    for line in inputFile:
        lines.append(line);

    inputFile.close();

    results = [];
    for i in range(0, len(lines)):
        # find the beginning of func definition
        result = re.findall(r'\w+::\w+\(.*\)?(?:\s?const)?[^;]\n$',lines[i]);
        if (result):
            # ensure we found func
            postResult = re.findall(r'^(?:\s)*(?:{|:)',lines[i+1]);
            resultENd = re.findall(r'{$',lines[i]);
            isCommentOk = (0 <= i-2) and is_empty(re.findall(r'\*/', lines[i-2]));
            if ((postResult or resultENd) and not isCommentOk):
                # try to find wrong comments before
                oldComments = [];
                for j in range(1, i):
                    delres = re.findall(r'^(?:(?://)|\s*$)', lines[i-j]);
                    if (delres):
                        oldComments.append(lines[i-j]);
                        results.pop();
                    else:
                        break;

                results += GetComment( ExtractComment(oldComments) );

        results.append(lines[i]);

    # writing into file
    outputFile = open('res.cpp','r+');
    outputFile.truncate(0);

    for result in results:
        outputFile.write(result)

    outputFile.close();

#--------------------------------------------------------------------------------------------------------------
def Removets(oldFileName, newFileName):

    # reading from file
    lines = [];
    inputFile = open(oldFileName,'r', encoding='utf-8');
    for line in inputFile:
        lines.append(line);

    inputFile.close();

    results = [];
    for i in range(0, len(lines)):
        lines[i] = lines[i].rstrip();

    # writing into file
    outputFile = open(newFileName,'w', encoding='utf-8');
    outputFile.truncate(0);

    for result in lines:
        outputFile.write(result + "\n")

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

#--------------------------------------------------------------------------------------------------------------
def check_file(lines):

    wrongLines = [];
    for i in range(0, len(lines)):
        if lines[i] != lines[i].rstrip():
            wrongLines.append(i)

    return wrongLines

#--------------------------------------------------------------------------------------------------------------
def fix_file(lines):

    inds = check_file(lines);
    for i in inds:
        lines[i] = lines[i].rstrip()

#--------------------------------------------------------------------------------------------------------------
def Test(newFile):

    # writing into file
    outputFile = open(newFile,'w', encoding='utf-8');
    outputFile.truncate(0);

    outputFile.write("Added\n")

    outputFile.close();