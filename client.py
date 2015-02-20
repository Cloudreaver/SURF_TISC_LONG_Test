#!/usr/bin/python
#
#
#
# ---------------------------------------------------------------------------

import socket
import sys
import time

class client :

      ACK_MESSAGE = "ACK"
      error_exit  = False
      link_open   = False

      DEFAULT_server_IP = 'anitarf.phys.hawaii.edu'
      DEFAULT_port      = 10000

      debug = False

      def __init__( self ) :                                                 # - create the TCP/IP client to server connection

          print "-> opening link to server : %s port %d" % ( self.DEFAULT_server_IP , self.DEFAULT_port )
          self.open_connection( self.DEFAULT_server_IP , self.DEFAULT_port )

          return

      def open_connection( self , server_node , server_port ) :              # - create the TCP/IP client to server connection

          self.error_exit = False

          if ( self.link_open ) :
              print '-> link already open ... no action taken'
              return self.error_exit

          self.sock = socket.socket( socket.AF_INET , socket.SOCK_STREAM )

          server_address = ( server_node , server_port )
          print >>sys.stderr, 'connecting to %s port %s' % server_address
          self.sock.connect( server_address )                                # Connect the socket to the port where the server is listening

          self.link_open  = True

          return self.error_exit

      def set_run( self , run_no ) :

          if ( not self.link_open ) :
             self.error_exit = True
             return self.error_exit

          self.error_exit = False
    
          string_runno = "SETRUN#%8.8d" % ( run_no )
          print >>sys.stderr, 'sending "%s"' % string_runno
          try :                                                              # - 1: send the event header
                                  self.sock.sendall( string_runno )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit
          try :                                                              # - 2: receive the acknowledge message
                                  data = self.sock.recv( 16 )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit
          if ( len(data) != 0 ) :                                            # - 3: verify the acknowledge message
             if ( data == self.ACK_MESSAGE ) :
                if ( self.debug ) :
                   print >>sys.stderr, 'received acknowledge message - "%s" - GOOD' % data
             else :
                print >>sys.stderr, 'received acknowledge message - "%s" - BAD'  % data
                self.error_exit = True
                return self.error_exit
          else :
             print >>sys.stderr, 'received zero-length acknowledge message - BAD'  % data
             self.error_exit = True
             return self.error_exit

          return self.error_exit

      def xmit_header( self , event_no ) :                                   # - transmit the event header record

          if ( not self.link_open ) :
             self.error_exit = True
             return self.error_exit

          self.error_exit = False
    
          string_header = "BEVENT#%8.8d" % ( event_no )
          print >>sys.stderr, 'sending "%s"' % string_header
          try :                                                              # - 1: send the event header
                                  self.sock.sendall( string_header )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit
          try :                                                              # - 2: receive the acknowledge message
                                  data = self.sock.recv( 16 )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit

          if ( len(data) != 0 ) :                                            # - 3: verify the acknowledge message
             if ( data == self.ACK_MESSAGE ) :
                if ( self.debug ) :
                   print >>sys.stderr, 'received acknowledge message - "%s" - GOOD' % data
             else :
                print >>sys.stderr, 'received acknowledge message - "%s" - BAD'  % data
                self.error_exit = True
                return self.error_exit
          else :
             print >>sys.stderr, 'received zero-length acknowledge message - BAD'  % data
             self.error_exit = True
             return self.error_exit

          return self.error_exit

      def xmit_bankno( self , bank_no ) :                                    # - transmit the bank number record

          if ( not self.link_open ) :
             self.error_exit = True
             return self.error_exit

          self.error_exit = False
    
          string_bankno = "BANK#%4.4d" % ( bank_no )
          print >>sys.stderr, 'sending "%s"' % string_bankno
          try :                                                              # - 1: send the event header
                                  self.sock.sendall( string_bankno )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit
          try :                                                              # - 2: receive the acknowledge message
                                  data = self.sock.recv( 16 )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit

          if ( len(data) != 0 ) :                                            # - 3: verify the acknowledge message
             if ( data == self.ACK_MESSAGE ) :
                if ( self.debug ) :
                   print >>sys.stderr, 'received acknowledge message - "%s" - GOOD' % data
             else :
                print >>sys.stderr, 'received acknowledge message - "%s" - BAD'  % data
                self.error_exit = True
                return self.error_exit
          else :
             print >>sys.stderr, 'received zero-length acknowledge message - BAD'  % data
             self.error_exit = True
             return self.error_exit

          return self.error_exit

      def xmit_data( self , this_tuple ) :                                   # transmit the waveform

          if ( not self.link_open ) :
             self.error_exit = True
             return self.error_exit

          self.error_exit = False

          bin_counter = 0
          for I in this_tuple :                                              # go through tuple, send each item
              element = "ELEMENT#%4.4d#%4.4d" % ( bin_counter , I )
              try :                                                          # - 1 : send data
                                  self.sock.sendall( element )
              except IOError :
                                  self.error_exit = True
                                  return self.error_exit
              try :                                                          # - 2 : receive the acknowledge message 
                                  data = self.sock.recv( 16 )
              except IOError :
                                  self.error_exit = True 
                                  return self.error_exit

              if ( len(data) != 0 ) :                                        # - 3: verify the acknowledge message
                 if ( data == self.ACK_MESSAGE ) :
                    if ( self.debug ) :
                       print >>sys.stderr, 'received acknowledge message - "%s" - GOOD' % data
                 else :
                    print >>sys.stderr, 'received acknowledge message - "%s" - BAD'  % data
                    self.error_exit = True
                    return self.error_exit
              else :
                 print >>sys.stderr, 'received zero-length acknowledge message - BAD'  % data
                 self.error_exit = True
                 return self.error_exit

              bin_counter = bin_counter + 1

          return self.error_exit

      def xmit_tail( self ) :                                                # - transmit the event tail record

          if ( not self.link_open ) :
             self.error_exit = True
             return self.error_exit

          self.error_exit = False
    
          string_tail = "EEVENT"
          print >>sys.stderr, 'sending "%s"' % string_tail
          try :                                                              # - 1: send the event tail
                                  self.sock.sendall( string_tail )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit
          try :                                                              # - 2: receive the acknowledge message
                                  data = self.sock.recv( 16 )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit

          if ( len(data) != 0 ) :                                            # - 3: verify the acknowledge message
             if ( data == self.ACK_MESSAGE ) :
                if ( self.debug ) : 
                   print >>sys.stderr, 'received acknowledge message - "%s" - GOOD' % data
             else :
                print >>sys.stderr, 'received acknowledge message - "%s" - BAD'  % data
                self.error_exit = True
                return self.error_exit
          else :
             print >>sys.stderr, 'received zero-length acknowledge message - BAD'  % data
             self.error_exit = True
             return self.error_exit

          return self.error_exit

      def close_connection( self ) :
                            
          self.error_exit = False

          if ( not self.link_open ) :
              print "-> link not open ... no action undertaken"
              return False

          print >>sys.stderr, 'closing socket'
          try : 
                                  self.sock.close( )
          except IOError :
                                  self.error_exit = True 
                                  return self.error_exit
          
          self.link_open = False

          return self.error_exit

      def reset_error_exit_flag( self ) :

          self.error_exit = False

          return

      def write_event( self , event_no , bank_no , data ) :

          header_write = self.xmit_header( event_no )
          bankno_write = self.xmit_bankno( bank_no )
          data_write   = self.xmit_data( data )
          end_write    = self.xmit_tail( )

          return

