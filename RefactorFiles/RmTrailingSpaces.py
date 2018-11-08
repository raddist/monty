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

    results = [];
    results.extend(lines)

    for i in inds:
        results[i] = results[i].rstrip() + "\n"

    return results