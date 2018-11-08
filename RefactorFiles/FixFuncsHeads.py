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
def check_file(lines):

    wrongLines = [];
    for i in range(0, len(lines)):
        # find the beginning of func definition
        result = re.findall(r'\w+::\w+\(.*\)?(?:\s?const)?[^;]\n$',lines[i]);
        if (result):
            # ensure we found func
            postResult = re.findall(r'^(?:\s)*(?:{|:)',lines[i+1]);
            resultENd = re.findall(r'{$',lines[i]);
            isCommentOk = (0 <= i-2) and is_empty(re.findall(r'\*/', lines[i-2]));

            if ((postResult or resultENd) and not isCommentOk):
                wrongLines.append(i)

    return wrongLines

#--------------------------------------------------------------------------------------------------------------
def fix_file(lines):

    inds = check_file(lines);
    
    results = [];
    for i in range(0, len(lines)):
        # find the beginning of func definition
        result = re.findall(r'\w+::\w+\(.*\)?(?:\s?const)?[^;]\n$',lines[i]);
        if i in inds :
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

    return results