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
Show details of a process accounting data file

Usage: show-acct -f <filename> -o <outfile> [-dhH]
	f: Filename (default /var/account/pacct)
	H: Supress header
	h: This help screen
	d: Delimited output (default="|")
	e: Supress processes with an exitcode of 0
	D: Turn on debug output
	o: Specify output file
	T: Calculate and show ending time field
	u: Show user ID in output
	0: Include processes with a run time of zero (excluded by default)
	v: Show accounting file version and exit

Flags: 
	X: Killed by signal
	C: Dumped core
	S: Ran with super-user privileges
	F: Executed fork but did not exec

	sysconf clockticks: 100

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
And another example

```
cbd@derp ~/sourcecode/show-acct $ ./show-acct -T -f ./pacct|head
             command       date    start      date      end  utime  stime     elapsed average_mem exitcode flag
             -------       ----    -----      ----      ---  ----- ------     ------- ----------- -------- ----
                bash   20160817 11:40:46  20160817 11:41:42   0.01   0.00       56.98       11080        0 --S-
                  su   20160817 11:40:43  20160817 11:41:42   0.02   0.00       59.72       63528        0 --S-
                 ssh   20160817 11:42:04  20160817 11:42:05   0.01   0.01        1.18       63408        0 ----
     yum-updatesd-he   20160817 11:43:03  20160817 11:43:10   4.01   0.28        7.63      285504        0 ----
                 vim   20160817 11:41:49  20160817 11:43:20   0.51   0.06       91.84       43344        0 ----
                 cc1   20160817 11:43:24  20160817 11:43:25   0.04   0.10        1.38       15696        0 ----
                  ld   20160817 11:43:26  20160817 11:43:26   0.01   0.01        0.20       10544        0 ----
                 vim   20160817 11:42:21  20160817 11:44:38   0.04   0.00      137.84       43352        0 ----
```
