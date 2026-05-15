# cli-improved
This is a collection of functional improvements to various Linux CLI commands, either as scripts that intercept `bin`-based binaries via alias, or as .bashrc functions that pre-empt them.

## what am i doing?
Linux commands have common options that we use all the time.  Think about it: we punch in `pwd` because we want to know the `cwd`, but what if it's a symbolic link, and we want to know exactly which files we're messing with?  You can do `pwd -L` followed by `pwd -P`, or I can write an intercept script (with .bashrc alias) that gives you both in this corner case, and only when you're in that corner case.  It might save your data, or your bacon, or both.  And what if you have several versions of an executable in your sprawling path: wouldn't you like to know the whole story when you `which` it?

## why am i doing this?
I have 52 years of CLI experience, every single day except my honeymoon and one Grand Canyon trip. Flag-waving and up-kissing sends the wrong message.  How about I practice "show, don't tell?"

## how am i doing this?
One day I printed two reams of manpages (easier than flipping screens) and started through them, tearing them apart.  Yeah, I've probably read most of them. Okay, maybe not `curl`, that page is both flippin' in-sane, and, to be fair, insanely well-written, but I have read a lot of it.

Anyway, I keep the stack on my desk next to my Mac (yes, I can do anything from a Mac that I can also do from my Ubuntu server that's hung from the bottom of a shelf in my closet and reachable from anywhere via tailscale).  Daily I sit down for some number of hours and go through the stack, looking for possible improvements that I can add with intercept scripting.  I post them here when I'm sure they're sane and reliable.  I also give you any `.bashrc` mods that make them work.

## what am i hoping to gain from this?
Well, initially, I couldn't even figure out what the heck I was doing and why I was doing it.  Something about doing it because I freakin' wanted to do it.  I'm not trying to be another smiling face trying to get attention; instead, I'm trying to do some good.

But also, I've aged out of the system at 67 and kinda "voluntired."  Retirement + social security *almost* gets it, but not exactly the lifestyle I want for as long as I estimate I'll want it (all my relatives died in their 90's). Toys are expensive, and I still want them.

So I wouldn't mind getting a lesser-salary job to make up the gap, doing things that I've already been doing for five decades.  Kinda like getting senior-level experience for the price of a junior.  I'm gonna do it anyway, but I'd like to get paid for it.  Please keep that in mind.


