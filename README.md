#Assignment 4 directory

This directory contains source code and other files for Assignment 4.

Is making a struct for the threads beneficial? If so, how?

How exactly are threads going to be communicating, i.e. how does the worker thread know when there is a request in the queue?

What do we do if the person send a response that is not supported? Do we add it to the audit log?






Conflict_Stress_Mix Test
====================================================================================
I checked how many inputs were being sent and how many times a connection was closed
both turned out to be 100.

I removed all the mutex locks as well as the flocks and the test still hangs

I print out what I push and pop and there are multiple instances where a number is pushed one time, but popped multiple times

Seems to only be popping around half of the 100 requests sent, sometimes it spikes to 60, other times it drops drastically

Every output on the audit log seems to be a 200 after a single 201 is sent, regardless if it's supposed to be a 404, 201, etc.


Automated
printf "PUT /foo.txt HTTP/1.1\r\nRequest-Id: 1\r\nContent-Length: 12\r\n\r\nHello World!" | ./cse130_nc localhost 1234