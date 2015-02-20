#!/usr/bin/python
#
#
# ------------------------------------------------------------

import surf_lab4_control_class
import time

this_lab = surf_lab4_control_class.SURF( 11 )
this_lab.SetClockSourceToFPGA( ) 
time.sleep( 0.5 )

#          this_lab.InitExtDACs_To_DevBoard2( )
#          time.sleep( 0.5 )
#          this_lab.SetVPed( 1600 )
#          time.sleep( 0.5 )
#          this_lab.SetXISE( 0xB00 )
#          time.sleep( 0.5 )
#          this_lab.InitIntDACs_To_DevBoard2( )
#          time.sleep( 0.5 )
#          this_lab.SetLoadDACsDone( )
#          time.sleep( 0.5 )
#          this_lab.cpci.surf_hold_source( 1 )    # 0 = TURF / 1 = CPCI

