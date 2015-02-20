#!/usr/bin/python
#
#
#
# --------------------------------------------------------------------------------------

import time

# -------------------------------------------------------------------------------------- #
#                       Define dictionary of LAB4B/C internal DACs                       #
# -------------------------------------------------------------------------------------- #
#                                                                                        #
#                internal_dacs["name"][0] - LAB4B/C register position                    #
#                internal_dacs["name"][1] - data mask                                    #
#                                                                                        #
# The register mapping was exacted from the update_inner_DAC.vhd firmware on 05/20/2014. #
# -------------------------------------------------------------------------------------- #

internal_dacs = { "LWRSTB"   : ( 0x080 , 0xff   ) , # LWRSTB
                  "TWRSTB"   : ( 0x081 , 0xff   ) , # TWRSTB
                  "LSSTdly"  : ( 0x082 , 0xff   ) , # LSSTdly (does not infleunce delay line functioning)
                  "TSSTdly"  : ( 0x083 , 0xff   ) , # LSSTdly (does not infleunce delay line functioning)
                  "LS1"      : ( 0x084 , 0xff   ) , # LS1
                  "TS1"      : ( 0x085 , 0xff   ) , # TS1
                  "LS2"      : ( 0x086 , 0xff   ) , # LS2
                  "TS2"      : ( 0x087 , 0xff   ) , # TS2
                  "LPHASE"   : ( 0x088 , 0xff   ) , # LPHASE
                  "TPHASE"   : ( 0x089 , 0xff   ) , # TPHASE
                  "LSSPin"   : ( 0x08A , 0xff   ) , # LSSPin
                  "TSSPin"   : ( 0x08B , 0xff   ) , # TSSPin
                  "TimeReg"  : ( 0x08C , 0xff   ) , # TimeReg
                  "WRPhase"  : ( 0x02  , 0xffff ) , # ChoicePhase
                  "PedSub"   : ( 0x03  , 0x0001 ) , # pedestal subtraction : off            (0) / on                  (1)
                  "PedUpd"   : ( 0x04  , 0x0001 ) , # updating pedestal    : no             (0) / yes                 (1)
                  "PedRead"  : ( 0x05  , 0x0001 ) } # reading pedestal     : no - read data (0) / yes - read pedestal (1)

# -------------------------------------------------------------------------------------- #
#                    Define dictionary of LAB4B/C external (I2C) DACs                    #
# -------------------------------------------------------------------------------------- #
#                                                                                        #
#                   external_dacs["name"][0] - I2C address for DAC device                #
#                   external_dacs["name"][1] - DAC output number                         #
#                   external_dacs["name"][2] - DAC number for S4A7 I2C write             #
#                   external_dacs["name"][3] - data mask                                 #
#                                                                                        #
# The I2C address and pin number for each DAC was obtained from the schematics posted on #
# the ANITA ELOG 518 (https://www.phys.hawaii.edu/elog/anita_notes/518).                 #
# -------------------------------------------------------------------------------------- #

external_dacs = { "Vbias2"     : ( 0x10 , 0x00 ,  0 , 0xfff ) , # [LTC2637]    - Vbias2
                  "XISE"       : ( 0x10 , 0x01 ,  1 , 0xfff ) , # [LTC2637]    - XISE
                  "SBbias"     : ( 0x10 , 0x02 ,  2 , 0xfff ) , # [LTC2637]    - SBbias
                  "CMPbias"    : ( 0x10 , 0x03 ,  3 , 0xfff ) , # [LTC2637]    - CMPbias
                  "VPed"       : ( 0x10 , 0x04 ,  4 , 0xfff ) , # [LTC2637]    - VPed
                  "VTimingThr" : ( 0x10 , 0x05 ,  5 , 0xfff ) , # [LTC2637]    - VTimingThr (for DOE->DOEp/DOEn translation)
                  "Vbs"        : ( 0x10 , 0x06 ,  6 , 0xfff ) , # [LTC2637]    - Vbs (for common VdlyN mode, this must be 0V!)
                  "VdlyP"      : ( 0x12 , 0x00 ,  8 , 0xfff ) , # [LTC2635_EP] - VdlyN   
                  "VdlyN"      : ( 0x12 , 0x01 ,  9 , 0xfff ) , # [LTC2635_EP] - VdlyP
                  "XROVDD"     : ( 0x12 , 0x02 , 10 , 0xfff ) , # [LTC2635_EP] - XROVDD
                  "Vbias"      : ( 0x12 , 0x03 , 11 , 0xfff ) } # [LTC2635_EP] - Vbias

# -------------------------------------------------------------------------------------- #
#                    Define dictionary of MONTIMING Output Selections                    #
# -------------------------------------------------------------------------------------- #
#                                                                                        #
#                                                                                        #
# -------------------------------------------------------------------------------------- #

TimeReg_values = { "A1"      : 0x00 ,
                   "B1"      : 0x01 ,
                   "A2"      : 0x02 ,
                   "B2"      : 0x03 ,
                   "PHASE"   : 0x04 ,
                   "PHASEAB" : 0x05 ,
                   "SSPin"   : 0x06 ,
                   "WRSTB"   : 0x07 ,
                   "SSPout"  : 0x44 ,
                   "SSTout"  : 0x64 }

iCE40_SPIBank = 2; # data-taking SPI bank ( 0=BOOT / 1=V14 / 2=V15 )

class SURF:

      import cpci

      lab_no    = 0
      cpci_open = 0
      TimeReg   = 0

      def __init__( self , input_lab_no ) :

          print "__init__( )                 : lab_no = %d" % ( input_lab_no )
          self.lab_no = input_lab_no
          try :            
                           cpci_status = self.cpci.open( )
                           if ( cpci_status == 0 ) :
                              print "__init__( )                 : cpci connection open success"
          except IOError :
                           print "__init__( )                 : cpci connection already open"
                           cpci_status = 0
          if ( cpci_status == 0 ) :
             board_id = self.cpci.read_bar1( 0 )
             if ( board_id == 0x53344137 ) : # 'S4A7'
                print "__init__( )                 : --> board_id = 0x%8.8x <-- 'S4A7' as expected for a SURFv4 board" % ( board_id )
                self.cpci_open = 1
                boot_firmware_id = self.cpci.surf_get_firmware_id( self.lab_no )
                print "__init__( )                 : --> iCE40 boot firmware id = 0x%4.4x" % ( boot_firmware_id )
                self.cpci.reboot_ice( self.lab_no , iCE40_SPIBank ) # reboot ICE to data-taking firmware 
                print "__init__( )                 : --> reboot iCE40 %2.2d to SPI bank %d" % ( self.lab_no , iCE40_SPIBank )
                acq_firmware_id = self.cpci.surf_get_firmware_id( self.lab_no )
                print "__init__( )                 : --> iCE40 acquisition firmware id = 0x%4.4x" % ( acq_firmware_id )
             else :
                print "__init__( )                 : --> board_id = 0x%8.8x <-- not 'S4A7' [PROBLEM!]" % ( board_id )
                self.cpci.close( )
                self.cpci_open = 0
          else :
             print "__init__( )                 : cpci connection open failed - error code = %d" % ( cpci_status )
             self.cpci_open = 0

          return # __init__() functions need to return nothing!

#     end of class function __init__( )
#     ----------------------------------------------------------

      def cpci_debug_off( self ) :

          new_debug_state = self.cpci.debugoff( )
          return

#     end of class function cpci_debug_off( )
#     ----------------------------------------------------------

      def cpci_debug_on( self ) :

          new_debug_state = self.cpci.debugon( )
          return

#     end of class function cpci_debug_off( )
#     ----------------------------------------------------------

      def write_dac( self , dac_name , value ) :

          if ( dac_name in internal_dacs ) :
             status = self.write_internal_dac( dac_name , value )
          elif ( dac_name in external_dacs ) :
             status = self.write_external_dac( dac_name , value )
          else :
             status = -1

          return status

#     end of class function write_dac( )
#     ----------------------------------------------------------

      def write_internal_dac( self , dac_name , value ) :

          if ( dac_name in internal_dacs ) :

             ( register_no , data_mask ) = internal_dacs[dac_name]
             value_to_write = value & data_mask
             if ( self.cpci_open == 1 ) :
                self.cpci.surf_lab_write( self.lab_no , register_no , value_to_write )
                print "write_internal_dac( )       : LAB %2d - internal DAC %7s - register 0x%4.4x <- 0x%4.4x" % ( self.lab_no , dac_name , register_no , value_to_write )
                return 0  # success
             else :
                print "write_internal_dac( )       : LAB %2d - internal DAC %7s - register 0x%4.4x <- 0x%4.4x [NOT DONE AS CPCI CONNECTION IS NOT OPEN]" % ( self.lab_no , dac_name , register_no , value_to_write )
                return -2 # failed since cpci connection has not been openned

          else :

             print "write_internal_dac( )       : LAB %2d - invalid internal DAC %s - no action undertaken" % ( self.lab_no , dac_name )
             return -1 # failure

#     end of class function write_internal_dac( )
#     ----------------------------------------------------------

      def write_wraddr_phase_register( self , value ) :

          if ( self.cpci_open == 1 ) :

             self.cpci.surf_set_wraddr_phase( self.lab_no , value )
             print "write_internal_dac( )       : LAB %2d - set \"write addr\" phase to 0x%4.4x (%d)" % ( self.lab_no , value , value )
             return 0  # success

          else :

             print "write_internal_dac( )       : LAB %2d - set \"write addr\" phase to 0x%4.4x (%d) <- 0x%4.4x [NOT DONE AS CPCI CONNECTION IS NOT OPEN]" % ( self.lab_no , value , value )
             return -2 # failed since cpci connection has not been openned

#     end of class function write_wraddr_phase_register( )
#     ----------------------------------------------------------

      def write_external_dac( self , dac_name , value ) :

          if ( dac_name in external_dacs ) :

             ( i2c_dac_addr , dac_pin_no , i2c_dac_no , data_mask ) = external_dacs[dac_name]
             value_to_write = value & data_mask
             if ( self.cpci_open == 1 ) :
                self.cpci.surf_i2c_write( self.lab_no , i2c_dac_no , value_to_write )
                print "write_external_dac( )       : LAB %2d - external DAC %7s - I2C address 0x%2.2x/pin %d [%2.2d] <- 0x%4.4x" % ( self.lab_no , dac_name , i2c_dac_addr , dac_pin_no , i2c_dac_no , value_to_write )
                return 0  # success
             else :
                print "write_external_dac( )       : LAB %2d - external DAC %7s - I2C address 0x%2.2x/pin %d [%2.2d] <- 0x%4.4x [NOT DONE AS CPCI CONNECTION IS NOT OPEN]" % ( self.lab_no , dac_name , i2c_dac_addr , dac_pin_no , i2c_dac_no , value_to_write )
                return -2 # failed since cpci connection has not been openned

          else :

             print "write_external_dac( )       : LAB %2d - invalid external (I2C) DAC %s - no action undertaken" % ( self.lab_no , dac_name )
             return -1 # failure

#     end of class function write_external_dac( )
#     ----------------------------------------------------------

      def SetWRSTB( self , leading , trailing ) :

          self.write_dac( "LWRSTB" , leading )    # LWRSTB
          self.write_dac( "TWTSTB" , trailing )   # TWRSTB

          return

#     end of class function SetWRSTB( )
#     ----------------------------------------------------------

      def SetS1( self , leading , trailing ) :

          self.write_dac( "LS1" , leading )       # LS1
          self.write_dac( "TS1" , trailing )      # TS1

          return

#     end of class function SetS1( )
#     ----------------------------------------------------------

      def SetS2( self , leading , trailing ) :

          self.write_dac( "LS2" , leading )       # LS2
          self.write_dac( "TS2" , trailing )      # TS2

          return

#     end of class function SetS2( )
#     ----------------------------------------------------------

      def SetPhase( self , leading , trailing ) :

          self.write_dac( "LPHASE" , leading )    # LPHASE
          self.write_dac( "TPHASE" , trailing )   # TPHASE

          return

#     end of class function SetPhase( )
#     ----------------------------------------------------------

      def SetSSPin( self , leading , trailing ) :

          self.write_dac( "LSSPin" , leading )    # LSSPin
          self.write_dac( "TSSPin" , trailing )   # TSSPin

          return

#     end of class function SetSSPin( )
#     ----------------------------------------------------------

      def SetChoicePhase( self , write_addr_phase ) :

          self.write_wraddr_phase_register( write_addr_phase )

          return

#     end of class function SetChoicePhase( )
#     ----------------------------------------------------------

      def SetTimeRegByNumber( self , output ) :

          self.write_dac( "TimeReg" , output )    # TimeReg
          self.TimeReg = output

          return

#     end of class function SetTimeRegByNumber( )
#     ----------------------------------------------------------

      def SetTimeReg( self , TimeRegName ) : 

          if ( TimeRegName in TimeReg_values ) :
             output = TimeReg_values[TimeRegName]
             self.write_dac( "TimeReg" , output ) # TimeReg
             self.TimeReg = output
          else :
             print "SetTimeReg( )               : LAB %2d - invalid TimeReg name (%s)" % ( self.lab_no , TimeRegName )

          return

#     end of class function SetTimeReg( )
#     ----------------------------------------------------------

      def StartPhaseFF( self ) :

          print "StartPhaseFF( )             : LAB %2d - starting Phase FF write (using TimeReg with value 0x%4.4x)" % ( self.lab_no , self.TimeReg )
          self.write_dac( "TimeReg" , ( 0xFF00 | self.TimeReg ) )
          self.write_dac( "TimeReg" , ( 0x0000 | self.TimeReg ) )

          return

#     end of class function StartPhaseFF( )
#     ----------------------------------------------------------

      def SetMonTimeThreshold( self , value ) :

          self.write_dac( "VTimingThr" , value )

          return

#     end of class function SetMonTimeThreshold( value )
#     ----------------------------------------------------------

      def SetVdlyN( self , value ) :

          self.write_dac( "VdlyN" , value )       # VdlyN

          return

#     end of class function SetVdlyN( )
#     ----------------------------------------------------------

      def SetVPed( self , value ) :

          self.write_dac( "VPed" , value )        # VPed

          return

#     end of class function SetVPed( )
#     ----------------------------------------------------------

      def SetVdlyP( self , value ) :

          self.write_dac( "VdlyP" , value )       # VdlyP
          return

#     end of class function SetVdlyP( )
#     ----------------------------------------------------------

      def SetXROVDD( self , value ) :

          self.write_dac( "XROVDD" , value )      # XROVDD

          return

#     end of class function SetXROVDD( )
#     ----------------------------------------------------------

      def SetVbias( self , value ) :

          self.write_dac( "Vbias" , value )       # Vbias

          return

#     end of class function SetVbias( )
#     ----------------------------------------------------------

      def SetVbias2( self , value ) :

          self.write_dac( "Vbias2" , value )      # Vbias2

          return

#     end of class function SetVbias2( )
#     ----------------------------------------------------------

      def SetXISE( self , value ) :

          self.write_dac( "XISE" , value )        # XISE

          return

#     end of class function SetXISE( )
#     ----------------------------------------------------------

      def SetVbs( self , value ) :

          self.write_dac( "Vbs" , value )         # Vbs

          return

#     end of class function SetVbs( )
#     ----------------------------------------------------------

      def SetClockSourceToFPGA( self ) :

          print "SetClockSourceToFPGA( )     : - setting clock source to FPGA via write of 2 to bar1 register 0x1c"
          self.cpci.write_bar1( 0x07 , 2 )        # clocksel (register 0x1c bit shifted by 4 to be 0x7)

          return

#     end of class function SetClockSourceFPGA( )
#     ----------------------------------------------------------

      def SetClockSourceToNoClock( self ) :

          print "SetClockSourceToFPGA( )     : - setting clock source to NoClock via write of 0 to bar1 register 0x1c"
          self.cpci.write_bar1( 0x07 , 0 )        # clocksel (register 0x1c bit shifted by 4 to be 0x7)

          return

#     end of class function SetClockSourceNoClock( )
#     ----------------------------------------------------------

      def SetLoadDACsDone( self ) :

          print "SetLoadDACsDone( )          : - have Artix synchronize LABs via write 0x1 to bar1 register 0xc0004 (not LAB dependent)"
          self.cpci.write_bar1( (0xc0004)>>2 , 1 )

          return

#     end of class function SetLoadDACsDone( )
#     ----------------------------------------------------------

      def InitAll_To_DevBoard2( self ) :

          self.SetClockSourceToFPGA( ) 
          time.sleep( 0.5 )
          self.InitExtDACs_To_DevBoard2( )
          time.sleep( 0.5 )
          self.SetVPed( 1600 )                    # put VPed around 1.0 V
          time.sleep( 0.5 )
          self.SetXISE( 0xB00 )                   # set XISE to ~1.6V
          time.sleep( 0.5 )
          self.InitIntDACs_To_DevBoard2( )
          time.sleep( 0.5 )
          self.SetLoadDACsDone( )                 # start WRADDR
          time.sleep( 0.5 )
          self.cpci.surf_hold_source( 1 )         # 0 = TURF / 1 = CPCI
          return

#     end of class function InitAll_To_DevBoard2( )
#     ----------------------------------------------------------

      def InitIntDACs_To_DevBoard2( self ) :

          print "InitExtDACs_To_DevBoard2( ) : ----------------------------------------------------------------"
          print "InitIntDACs_To_DevBoard2( ) : setting internal DACs of LAB %2d to DevBoard 2 values" % ( self.lab_no )
          print "InitExtDACs_To_DevBoard2( ) : ----------------------------------------------------------------"

          self.write_dac( "LWRSTB"  , 0x0054 )    # LWRSTB
          self.write_dac( "TWRSTB"  , 0x0012 )    # TWRSTB

          self.write_dac( "LS1"     , 0x001C )    # LS1
          self.write_dac( "TS1"     , 0x0047 )    # TS1

          self.write_dac( "LS2"     , 0x005B )    # LS2
          self.write_dac( "TS2"     , 0x0004 )    # TS2

          self.write_dac( "LPHASE"  , 0x0006 )    # LPHASE
          self.write_dac( "TPHASE"  , 0x0020 )    # TPHASE

          self.write_dac( "LSSPin"  , 0x0060 )    # LSSPin
          self.write_dac( "TSSPin"  , 0x0010 )    # TSSPin

          self.write_dac( "TimeReg" , TimeReg_values["PHASEAB"] )  # TimeReg (was 0x64, needs to be "PHASEAB" = 0x05)
          self.TimeReg = TimeReg_values["PHASEAB"]

          self.write_wraddr_phase_register( 15 )  # ChoicePhase

          self.StartPhaseFF( )                    # turn on the LAB

          return

#     end of class function InitIntDACs_To_DevBoard2( )
#     ----------------------------------------------------------

      def InitIntDACs_To_AllZeros( self ) :

          print "InitIntDACs_To_AllZeros( )  : ----------------------------------------------------------------"
          print "InitIntDACs_To_AllZeros( )  : setting internal DACs of LAB %2d to zero values" % ( self.lab_no )
          print "InitIntDACs_To_AllZeros( )  : ----------------------------------------------------------------"

          self.write_dac( "LWRSTB"  , 0x0000 )    # LWRSTB
          self.write_dac( "TWRSTB"  , 0x0000 )    # TWRSTB

          self.write_dac( "LS1"     , 0x0000 )    # LS1
          self.write_dac( "TS1"     , 0x0000 )    # TS1

          self.write_dac( "LS2"     , 0x0000 )    # LS2
          self.write_dac( "TS2"     , 0x0000 )    # TS2

          self.write_dac( "LPHASE"  , 0x0000 )    # LPHASE
          self.write_dac( "TPHASE"  , 0x0000 )    # TPHASE

          self.write_dac( "LSSPin"  , 0x0000 )    # LSSPin
          self.write_dac( "TSSPin"  , 0x0000 )    # TSSPin

          self.write_dac( "TimeReg" , 0x0000 )    # TimeReg
          self.TimeReg = 0x0000

          self.write_dac( "ChoicePhase" , 0  )    # ChoicePhase

          self.StartPhaseFF( )                    # turn on the LAB

          return

#     end of class function InitIntDACs_To_AllZeros( )
#     ----------------------------------------------------------

      def InitExtDACs_To_DevBoard2( self ) :
      
          print "InitExtDACs_To_DevBoard2( ) : ----------------------------------------------------------------"
          print "InitExtDACs_To_DevBoard2( ) : setting external DACs of LAB %2d to DevBoard 2 values" % ( self.lab_no )
          print "InitExtDACs_To_DevBoard2( ) : ----------------------------------------------------------------"

          self.write_dac( "Vbias"   , self.devboard_to_surf_ltc2635ep_dac_value( 0x0440 ) )  # Vbias
          self.write_dac( "Vbias2"  , self.devboard_to_surf_ltc2637_dac_value(   0x0400 ) )  # Vbias2
          self.write_dac( "VdlyN"   , self.devboard_to_surf_ltc2635ep_dac_value( 0x0628 ) )  # VdlyN

          self.write_dac( "VdlyP"   , self.devboard_to_surf_ltc2635ep_dac_value( 0x0AA8 ) )  # VdlyP
          self.write_dac( "XROVDD"  , self.devboard_rovdd_to_surf_dac_value(     0x0800 ) )  # XROVDD

          self.write_dac( "CMPbias" , self.devboard_to_surf_ltc2637_dac_value(   0x0500 ) )  # CMPbias
          self.write_dac( "SBbias"  , self.devboard_to_surf_ltc2637_dac_value(   0x0546 ) )  # SBbias

          self.write_dac( "XISE"    , self.devboard_to_surf_ltc2637_dac_value(   0x0900 ) )  # XISE

          self.write_dac( "Vbs"     , self.devboard_to_surf_ltc2637_dac_value(   0x0000 ) )  # Vbs (for common VdlyN mode, this must be 0V!)

          self.SetMonTimeThreshold( 1147 )      # 700 mV as per Patrick's email [05/20/2014]

          return

#     end of class function InitExtDACs_To_DevBoard2( )
#     ----------------------------------------------------------

      def devboard_rovdd_to_surf_dac_value( self , value ) :

          volts     = 1.250 + ( value / 4096.0 ) * 2.5
          dac_value = int( ( volts / 2.5 ) * 4096 )
          if ( dac_value > 4095 ) :
             dac_value = 4095
          if ( dac_value < 0 ) :
             dac_value = 0

          if ( volts > 2.5 ) : # protect LAB from overvoltage mistake
             raise ValueError( 'voltage exceeds 2.5V' )
          if ( volts < 0.0 ) : # negative voltages do not make sense
             raise ValueError( 'voltage below 0.0V' )

          print "devboard_rovdd_to_surf_dac_value() - input value = 0x%4.4x (%d) -> %f volts -> output_value = 0x%4.4x (%d)" % ( value , value , volts , dac_value , dac_value )

          return dac_value

#     end of class function devboard_rovdd_to_surf_dac_value( )
#     ----------------------------------------------------------

      def devboard_to_surf_ltc2637_dac_value( self , value ) :

          volts     = ( value / 4096.0 ) * 2.5
          dac_value = int( ( volts / 2.5 ) * 4096 )
          if ( dac_value > 4095 ) :
             dac_value = 4095
          if ( dac_value < 0 ) :
             dac_value = 0

          if ( volts > 2.5 ) : # protect LAB from overvoltage mistake
             raise ValueError( 'voltage exceeds 2.5V' )
          if ( volts < 0.0 ) : # negative voltages do not make sense
             raise ValueError( 'voltage below 0.0V' )

          print "devboard_to_surf_ltc2637_dac_value() - input value = 0x%4.4x (%d) -> %f volts -> output_value = 0x%4.4x (%d)" % ( value , value , volts , dac_value , dac_value )

          return dac_value

#     end of class function devboard_to_surf_ltc2637_dac_value( )
#     ----------------------------------------------------------

      def devboard_to_surf_ltc2635ep_dac_value( self , value ) :

          volts     = ( value / 4096.0 ) * 2.5
          dac_value = int( volts * 1000.0 )
          if ( dac_value > 4095 ) :
             dac_value = 4095
          if ( dac_value < 0 ) :
             dac_value = 0

          if ( volts > 2.5 ) : # protect LAB from overvoltage mistake
             raise ValueError( 'voltage exceeds 2.5V' )
          if ( volts < 0.0 ) : # negative voltages do not make sense
             raise ValueError( 'voltage below 0.0V' )

          print "devboard_to_surf_ltc2635ep_dac_value() - input value = 0x%4.4x (%d) -> %f volts -> output_value = 0x%4.4x (%d)" % ( value , value , volts , dac_value , dac_value )

          return dac_value

#     end of class function devboard_to_surf_ltc2635ep_dac_value( )
#     ----------------------------------------------------------

      def SetDAC2637_To_DevBoard2( self ) :

          self.write_dac( "Vbias2"     , self.devboard_to_surf_ltc2637_dac_value( 0x0400 ) )  # Vbias2     ( 0 )
          self.write_dac( "XISE"       , self.devboard_to_surf_ltc2637_dac_value( 0x0900 ) )  # XISE       ( 1 )
          self.write_dac( "SBbias"     , self.devboard_to_surf_ltc2637_dac_value( 0x0546 ) )  # SBbias     ( 2 )
          self.write_dac( "CMPbias"    , self.devboard_to_surf_ltc2637_dac_value( 0x0500 ) )  # CMPbias    ( 3 )
          self.write_dac( "VPed"       ,  100 )                                               # VPed       ( 4 )
          self.write_dac( "VTimingThr" , 1147 )                                               # VTimingThr ( 5 )
          self.write_dac( "Vbs"        , self.devboard_to_surf_ltc2637_dac_value( 0x0000 ) )  # Vbs        ( 6 )

          return

#     end of class function SetDAC2637_To_DevBoard2( )
#     ----------------------------------------------------------

      def SetDAC2635_To_DevBoard2( self ) :

          self.write_dac( "VdlyP"   , self.devboard_to_surf_ltc2635ep_dac_value( 0x0AA8 ) )   # VdlyP  ( 0 )
          self.write_dac( "VdlyN"   , self.devboard_to_surf_ltc2635ep_dac_value( 0x0628 ) )   # VdlyN  ( 1 )
          self.write_dac( "XROVDD"  , self.devboard_rovdd_to_surf_dac_value(     0x0800 ) )   # XROVDD ( 2 )
          self.write_dac( "Vbias2"  , self.devboard_to_surf_ltc2637_dac_value(   0x0400 ) )   # Vbias2 ( 3 )

          return

#     end of class function SetDAC2635_To_DevBoard2( )
#     ----------------------------------------------------------

