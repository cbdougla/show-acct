# show-acct
Utility to dump information from process accounting

A while back, I needed a utility dump the process accounting files from UNIX/Linux in a human readable format.<br>
This is the result.<br>
<br>
By default it outputs a table of commands but can also output a delimited file for importing into a spreadsheet or database.
<br>
It will handle version 2 or 3 of the psacct file.
<br>


```
[cbd][jordan][/devl/cbd/sourcecode/show-acct]$ show-acct -h

Show details of a process accounting data file

Usage: show-acct -f <filename> -o <outfile> [-dhH]
        f: Filename (default /var/account/pacct)
        H: Supress header
        h: This help screen
        d: Delimited output (default="|")
        e: Supress processes with an exitcode of 0
        D: Turn on debug output
        o: Specify output file
        u: Show user ID in output
        0: Include processes with a run time of zero (excluded by default)
        v: Show accounting file version and exit

Flags: 
        X: Killed by signal
        C: Dumped core
        S: Ran with super-user privileges
        F: Executed fork but did not exec
```      

Here's an example of the output

```
root@jordan:account> # show-acct -f ./pacct.1|head 
             command       date    start  utime  stime     elapsed average_mem exitcode flag
             -------       ----    -----   ---- ------     ------- ----------- -------- ----
                sort   20170321 04:02:03      1      0           3        3848        0 ----
                sort   20170321 04:02:03      1      0           3        3848        0 ----
                sort   20170321 04:02:03      1      0           3        3848        0 ----
                sort   20170321 04:02:03      1      0           3        3848        0 ----
            updatedb   20170321 04:02:03     12     38         958        5968        0 --S-
                rpmq   20170321 04:02:12    188      5         316       59216        0 ----
                  ps   20170321 04:02:50      1      1           2       12552        0 ----
                  ps   20170321 04:04:50      1      1           2       12552        0 ----
```
