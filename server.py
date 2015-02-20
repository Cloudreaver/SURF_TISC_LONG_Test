#!/usr/bin/python
#
#
#
# -------------------------------------------------------------------------------------------

import socket
import sys

class server :

      ACK_MESSAGE = "ACK"

      DEFAULT_server_IP = 'gluon.phys.hawaii.edu'
      DEFAULT_port      = 10000

      file_is_open      = False

      this_run_no       = 0
      this_event_no     = 0
      this_bank_no      = 0
      this_event_data   = []

      def __init__( self ) :

          self.client_address = 'UNKNOWN'
          self.bind_to_port( self.DEFAULT_server_IP , self.DEFAULT_port )

          return

      def bind_to_port( self , server_ip , server_port ) :

          self.sock = socket.socket( socket.AF_INET , socket.SOCK_STREAM ) # Bind the socket to the port

          self.server_address = ( server_ip , server_port )
          print >>sys.stderr, 'starting up on %s port %s' % self.server_address
          self.sock.bind( self.server_address )

          return

      def wait_for_connection( self ) :

          self.sock.listen( 1 )

          print >>sys.stderr, 'waiting for a connection'
          self.connection , self.client_address = self.sock.accept( )
          print >>sys.stderr, '-> connection from', self.client_address

          self.connection_active( )

          return

      def connection_active( self ) :

          while True:
                data = self.connection.recv( 20 )
                print >>sys.stderr, '-> received "%s"' % data
                if data :
                    print >>sys.stderr, '-> sending data back to the client'
                    self.connection.sendall( self.ACK_MESSAGE )
                    items = data.split( "#" )
                    if ( items[0] == 'SETRUN' ) :
                       self.this_run_no = int( items[1] )
                    elif ( items[0] == 'BEVENT' ) :
                       self.this_event_no = int( items[1] )
                       self.this_event_data = []
                       print "Start Event %d" % ( self.this_event_no )
                    elif ( items[0] == 'BANK' ) :
                       self.this_bank_number = int( items[1] )
                       print "-> bankno = %d" % ( self.this_bank_number )
                    elif ( items[0] == 'ELEMENT' ) :
                       bin = int( items[1] )
                       val = int( items[2] )
                       self.this_event_data.append( ( bin , val ) ) 
                    elif ( items[0] == 'EEVENT' ) :
                       print "Received event %d" % ( self.this_event_no )
                       print self.this_event_data
                       self.write_event( )
                    else :
                       print "INVALID INSTRUCTION"
                else:
                    print >>sys.stderr, '-> no more data from', self.client_address
                    self.close_connection( )
                    break

      def write_event( self ) :

          if ( self.file_is_open ) :
             print "-> unable to write event - file is already open and that is strange"
             return False

          self.event_file = ''.join( ['./SURFv4_event_file.run_' , str( self.this_run_number ) , '.event_' , str( self.this_event_no ) , '.bank_' , str( self.this_bank_number ) , '.dat' ] )
#         print "-> event_file = \"%s\"" % ( self.event_file )
          try :
                          FP = open( self.event_file , 'w' )
          except IOError:
                          print "-> unable to open output event file: \"%s\"" % ( self.event_file )
          else :
                          self.file_is_open = True
                          print "-> open: file %s" % ( self.event_file )
          if ( self.file_is_open ) :
             line_counter = 0
             for bin_value_pair_list in self.this_event_data :
                 output_line = ''.join( [ '%d ' % ( line_counter ) , '%d %d' % bin_value_pair_list , '\n' ] )
                 FP.write( output_line )
                 line_counter = line_counter + 1
             FP.close( )
             print "-> wrote %d lines to %s file" % ( line_counter , self.event_file )
             self.file_is_open = False

          return

      def close_connection( self ) :
          
          print "closing connection"
          self.connection.close()
          self.client_address = 'UNKNOWN'

