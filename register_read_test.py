#!/usr/bin/python
#
# This script repeatly reads from register 0x80 of the TISC
# board and checks to see if the returned value is 'GLIT'
# as expected.  When it is not, it reports an error.
#
# ---------------------------------------------------------

import time
import cpci
import sys

GLITZ=0
ADDR=0x80

print "register_read_test.py : repeatly read from register 0x%4.4x of glitz %d and verify that returned value is 'GLITZ'" % ( ADDR , GLITZ )
sys.stdout.flush()

cpci.debugoff( )

cpci.open( )

good = 0
bad  = 0

for I in range( 0 , 10000 ) :
#    time.sleep( 5 )
    J = cpci.read_glitz( GLITZ , ADDR )
    if ( J != 1196181844 ) :
       bad = bad + 1
       print "%8d -- 0x%8.8x not 0x%8.8x" % ( I , J , 1196181844 )
    else :
       good = good + 1
    sys.stdout.flush( )

print "Good: %d --- Bad: %d" % ( good , bad )

cpci.close( )

cpci.debugon( )

