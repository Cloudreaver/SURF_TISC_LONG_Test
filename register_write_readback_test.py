#!/usr/bin/python
#
# This script writes sequential numbers to register 0x82
# of the TISC board and checks to see if the readback
# from the register returns the written value.  If it does
# not, the script reports an error.
#
# ---------------------------------------------------------

import time
import cpci
import sys

GLITZ=0
#ADDR=0x82
ADDR=0x3F8F

print "register_write_and_readback_test.py : repeatly write and readback from register 0x%4.4x of glitz %d and verify that returned value is as expected" % ( ADDR , GLITZ )
sys.stdout.flush()

cpci.debugoff( )

cpci.open( )

good = 0
bad  = 0

I=0
while( I < 0xFFFFFFFF ) :
#for ( I=0 ; I<10000000 ; I++ ) :
# in range( 0 , 1000000 ) :
#    time.sleep( 0.00001 ) # 0.01 millisecond delay
    STATUS = cpci.write_glitz( GLITZ , ADDR , I )
    J = cpci.read_glitz( GLITZ , ADDR )
    if ( J != I ) :
       bad = bad + 1
       print "%8d -- 0x%8.8x not 0x%8.8x" % ( I , J , I )
    else :
       good = good + 1
#      print "%8d -- 0x%8.8x == 0x%8.8x" % ( I , J , I )
    sys.stdout.flush( )
    if ( (I % 1000) == 0 ) :
       print "I = 0x%8.8x [%12.12d] ... Good: %d --- Bad: %d" % ( I , I , good , bad )
    I = I + 1

print "Good: %d --- Bad: %d" % ( good , bad )

cpci.close( )

cpci.debugon( )


