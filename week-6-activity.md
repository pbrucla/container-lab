# Week 6 activity: Networking

In this week's activity we will implement limited networking functionality for our container runtime. 
We build upon the code from week 4 and implement functionality which enables synchronous UDP sending and receipt from within the jail. 

## `test-udp`

This program can be run **inside** the jail to test if the synchronous transmission of UDP packets into and out of the jail is functioning. 

It simply sends a DNS request to 1.1.1.1 to resolve `acmcyber.com`, and verifies that the reply UDP packet received is as expected. It prints the IP addresses resolved. 

If everything works as intended, `acmcyber.com` should refer to the following IP addresses
1. 185.199.108.153
2. 185.199.109.153
3. 185.199.110.153
4. 185.199.111.153