#!/usr/bin/python
#
#
#
# -------------------------------------------------------------------------------------

import surf_lab4_control_class as a 
import client as c

class surf_daq :

      lab_no   = 11

      run_no   = 0
      run_open = False

      event_no = 0
      bank_no  = 0

      have_new_event = False

      import pylab
      already_have_a_plot_up = False

      def __init__( self , this_lab_no ) :

          self.lab_no = this_lab_no
          return_code = self.read_runno( )
          return_code = self.start_daq( )

          self.event_no = 0

          self.already_have_a_plot_up = False

          return

      def read_runno( self ) :

          if ( self.run_open ) :
             print "read_runno( ) : -> ERROR: run already open"
             return False

          try : 
                            FP = open( 'run_number_file' , 'r' )
          except IOError:
                            print "read_runno( ) : -> ERROR: unable to open run_number_file for read"
                            return False

          try :
                            self.run_no = int( FP.read( ) )
          except IOError:
                            print "read_runno( ) : -> ERROR: unable to read run_number_file"
                            return False
                            
          FP.close( )

          print "read_runno( ) : run_no = %d" % ( self.run_no )

          try : 
                            FP = open( 'run_number_file' , 'w' )
          except IOError:
                            print "read_runno( ) : -> ERROR: unable to open run_number_file for write (to advance run number)"
                            return False

          next_run_no = '%d' % ( self.run_no + 1 )

          try :
                            FP.write( next_run_no )
          except IOError:
                            print "read_runno( ) : -> ERROR: unable to write next run number to run_number_file"
                            return False

          FP.close( )

          self.run_open = True

          return True

      def set_runno_by_hand( self , this_run_no ) :

          self.run_no = this_run_no 
          self.run_open = True

          print "set_runno_by_hand( ) : run_no = %d" % ( self.run_no )

          return True

      def start_daq( self ) :

          self.daq = c.client( )
          if ( not self.run_open ) :
             return_code = self.read_runno( )
             if ( not return_code ) :
                print "start_daq( ) : -> ERROR: run number not set - do it by hand"
                return False

          self.lab11 = a.SURF( self.lab_no )
          self.lab11.InitAll_To_DevBoard2( )

          return True

      def stop_daq( self ) :

          self.daq.close_connection( )

          return True

      def read_surf_lab( self ) :

          self.bank_no = 0

          self.lab11.cpci.surf_full_digitize_lab_bank( 0x1 , 0 )
          self.event_data = self.lab11.cpci.surf_read_data( 11 , 0 )

          self.lab11.cpci.surf_full_digitize_lab_bank( 0x2 , 1 )     # hold/digitize past the other three banks
          self.lab11.cpci.surf_full_digitize_lab_bank( 0x4 , 2 )
          self.lab11.cpci.surf_full_digitize_lab_bank( 0x8 , 3 )

          self.have_new_event = True
          self.event_no = self.event_no + 1

          return True

      def xmit_event( self ) :

          if ( not self.have_new_event ) :
             print "-> no new event in memory"
             return False

          if ( len(self.event_data) != 1024 ) :
             print "-> no or incomplete event in memory - size = %d" % ( len(self.event_data) )
             return False
              
          self.daq.write_event( self.event_no , self.bank_no , self.event_data )
          self.have_new_event = False

          return True

      def latch_pedestal_event( self ) :

          A = [ ]
          for I in range( 0 , len(self.event_data) , 1 ) :
              A.append( self.event_data[I] )
          self.pedestal_data = tuple( A )
          del A

          return True

      def pedestal_subtract( self ) :

          if ( len(self.pedestal_data) != 1024 ) :
             print "pedestal_subtract( ) : ERROR - no pedestal data in memory"
             return False

          A = [ ]
          for I in range( 0 , len(self.event_data) , 1 ) :
              A.append( self.pedestal_data[I] - self.event_data[I] )
          self.pedestal_subtracted_data = tuple( A )
          del A

          return True

      def show_event_data( self , Overlay_Flag = True ) :

          if ( self.already_have_a_plot_up ) :
             if ( not Overlay_Flag ) :
                self.pylab.clf( )
          else :
             self.fig = self.pylab.figure( )

          if ( not Overlay_Flag or not self.already_have_a_plot_up ) :
             self.ax  = self.fig.add_subplot( 111 )

          self.ax.plot( self.event_data ) 
          self.fig.show( )

          self.already_have_a_plot_up = True


