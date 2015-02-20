
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

#include "Python.h"

#include "cpcitest.h"

#if 0
#define CPCITEST_BAR1_SIZE 256
#define CPCITEST_BAR0_SIZE 4096
#define CPCITEST_CFG_SIZE  256
#endif

#if 0 /* LONG */
#define CPCITEST_BAR1_SIZE 2048*1024 /* do not divide by 4 as the register range comparisons include a divide by sizeof(int) */
#define CPCITEST_BAR0_SIZE 4096
#define CPCITEST_CFG_SIZE  256
#endif

#if 0 /* TISC */
#define CPCITEST_BAR1_SIZE 2048*1024 /* do not divide by 4 as the register range comparisons include a divide by sizeof(int) */
#define CPCITEST_BAR0_SIZE 4096
#define CPCITEST_CFG_SIZE  256
#endif

#if 1 /* SURF */
#define CPCITEST_BAR1_SIZE 1024*1024 /* do not divide by 4 as the register range comparisons include a divide by sizeof(int) */
#define CPCITEST_BAR0_SIZE 4096
#define CPCITEST_CFG_SIZE  256
#endif

int cpcitest_open(cpcitest_h *dev) {
  //  int pcifd;
  if (!dev) return CPCITEST_ERR_INVALID_HANDLE;
  dev->valid = 0;
  dev->cfg_fd = open("/sys/class/uio/uio0/device/config", O_RDWR | O_SYNC);
  if (dev->cfg_fd < 0) {
    perror("cpci: open cfg");
    return CPCITEST_ERR_OPEN;
  }
  dev->bar0_fd = open("/sys/class/uio/uio0/device/resource0", O_RDWR | O_SYNC);
  if (dev->bar0_fd < 0) {
    perror("cpci: open bar0");
    close(dev->cfg_fd);
    return CPCITEST_ERR_OPEN;
  }
  dev->bar1_fd = open("/sys/class/uio/uio0/device/resource1", O_RDWR | O_SYNC);
  if (dev->bar1_fd < 0) {
    perror("cpci : open bar1");
    close(dev->bar0_fd);
    close(dev->cfg_fd);
    return CPCITEST_ERR_OPEN;
  }

  // mmap
  dev->bar0 = mmap(NULL, CPCITEST_BAR0_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev->bar0_fd, 0);
  if (dev->bar0 == MAP_FAILED) {
    perror("cpci : mmap bar0");
    close(dev->cfg_fd);
    close(dev->bar0_fd);
    close(dev->bar1_fd);
    return CPCITEST_ERR_OPEN;
  }
  dev->bar1 = mmap(NULL, CPCITEST_BAR1_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev->bar1_fd, 0);
  if (dev->bar1 == MAP_FAILED) {
    perror("cpci : mmap bar1");
    munmap(dev->bar0, CPCITEST_BAR0_SIZE);
    close(dev->cfg_fd);
    close(dev->bar0_fd);
    close(dev->bar1_fd);
    return CPCITEST_ERR_OPEN;
  }
  dev->valid = 1;
  return CPCITEST_SUCCESS;
}

void cpcitest_close(cpcitest_h *dev) {
  if (!dev) return;
  if (!dev->valid) return;   
  munmap(dev->bar0, CPCITEST_BAR0_SIZE);
  munmap(dev->bar1, CPCITEST_BAR1_SIZE);
  close(dev->cfg_fd);
  close(dev->bar0_fd);
  close(dev->bar1_fd);
}

unsigned int cpcitest_bar0_read(cpcitest_h *dev, int regNum) {
  if (!dev) return CPCITEST_ERR_INVALID_HANDLE;
  if (!dev->valid) return CPCITEST_ERR_INVALID_HANDLE;
  if (regNum < 0 || regNum > (CPCITEST_BAR0_SIZE/sizeof(int))) return CPCITEST_ERR_INVALID_REGISTER;
  return *(((unsigned int *) dev->bar0) + regNum);
}

int cpcitest_bar0_write(cpcitest_h *dev, int regNum, unsigned int value) {
  if (!dev) return CPCITEST_ERR_INVALID_HANDLE;
  if (!dev->valid) return CPCITEST_ERR_INVALID_HANDLE;
  if (regNum < 0 || regNum > (CPCITEST_BAR0_SIZE/sizeof(int))) return CPCITEST_ERR_INVALID_REGISTER;
  *(((unsigned int *) dev->bar0) + regNum) = value;
  return 0;
}

unsigned int cpcitest_bar1_read(cpcitest_h *dev, int regNum) {
  if (!dev) return CPCITEST_ERR_INVALID_HANDLE;
  if (!dev->valid) return CPCITEST_ERR_INVALID_HANDLE;
  if (regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int))) return CPCITEST_ERR_INVALID_REGISTER;
  return *(((unsigned int *) dev->bar1) + regNum);
}

int cpcitest_bar1_write(cpcitest_h *dev, int regNum, unsigned int value) {
  if (!dev) return CPCITEST_ERR_INVALID_HANDLE;
  if (!dev->valid) return CPCITEST_ERR_INVALID_HANDLE;
  if (regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int))) return CPCITEST_ERR_INVALID_REGISTER;
  *(((unsigned int *) dev->bar1) + regNum) = value;
  return 0;
}

static unsigned int cpci_device_open = 0; /* == 0 (C false), not open / != 0 (C true), open                                */
static cpcitest_h   dev;
static unsigned int debug_output = 1;     /* == 0 (C false), produce NO debug output / != 0 (C true), produce debug output */
static unsigned int current_bank = 0;     /* current storage capacitor bank for all LABs into which the sampling capacitors are writing */

static          char *ice40_firmware_file_name    = NULL; /* name of firmware file in memory                 */
static unsigned char *ice40_firmware_buffer       = NULL; /* pointer to memory containing the ICE40 firmware */
static unsigned int   ice40_firmware_buffer_size  = 0;    /* size of the ICE40 firmware file                 */

static unsigned char *firmware_bank_image         = NULL;
static unsigned int   firmware_bank_image_size    = 0;
static unsigned int   firmware_bank_image_bank_no = -1;


static PyObject *python_cpci_debug_on( PyObject *self ) {

  debug_output = 1;
  return Py_BuildValue( "i" , 1 );

}

static PyObject *python_cpci_debug_off( PyObject *self ) {

  debug_output = 0; /* == 0, produce NO debug output */

  return Py_BuildValue( "i" , 0 );

}

static PyObject *python_cpci_open( PyObject *self ) {

  int ret;

  if ( ! cpci_device_open ) {
     ret = cpcitest_open( &dev );
     if ( ret != CPCITEST_SUCCESS ) {
        PyErr_SetString( PyExc_IOError , "Unable to open uio_pci_generic cpci device" );
        return NULL;
     } else {
        cpci_device_open = 1;   /* == 0 (C false), not open / != 0 (C true), open                           */
     }
  } else {
        PyErr_SetString( PyExc_IOError , "cpci device already open" );
        return NULL;
  }

  return Py_BuildValue( "i" , 0 );

}

static PyObject *python_cpci_close( PyObject *self ) {

  cpcitest_close( &dev );
  cpci_device_open = 0;    /* == 0 (C false), not open / != 0 (C true), open                                */

  return Py_BuildValue( "i" , 0 );

}

void python_cpci_atexit( ) {

  printf("python_cpci_atexit() : on python exit, closing the uic_pci_generic device\n");
  cpcitest_close( &dev );
  printf("python_cpci_atexit() : on python exit, freeing memory allocated to store ICE40 firmware\n");
  if ( ice40_firmware_buffer != NULL ) {
     printf("python_cpci_atexit() : on python exit, freeing memory allocated to store ICE40 firmware\n");
     fflush(stdout);
     free( ice40_firmware_buffer );
     ice40_firmware_buffer = NULL;
     ice40_firmware_buffer_size = 0;
  }
  return;

}

static PyObject *python_cpci_read_bar0( PyObject *self , PyObject *args ) {

  int regNum = 0;
  if ( !PyArg_ParseTuple( args , "i" , &(regNum) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( regNum )" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR0_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar0_read( &dev, regNum );
  printf("python_cpci_read_bar0() : BAR0 0x00: %8.8X (%c%c%c%c) PYTHON\n", idval,
         (idval >> 24) & 0xFF,
         (idval >> 16) & 0xFF,
         (idval >> 8) & 0xFF,
         idval & 0xFF);

  return Py_BuildValue( "I" , idval );

}

static PyObject *python_cpci_write_bar0( PyObject *self , PyObject *args ) {

  int          regNum = 0;
  unsigned int  value = 0;
  if ( !PyArg_ParseTuple( args , "iI" , &(regNum) , &(value) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( regNum , value )" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR0_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  int idval = cpcitest_bar0_write( &dev, regNum , value );

  return Py_BuildValue( "i" , idval );

}

static PyObject *python_cpci_read_bar1( PyObject *self , PyObject *args ) {

  int regNum = 0;
  if ( !PyArg_ParseTuple( args , "i" , &(regNum) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( regNum )" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_read(&dev, regNum);
  printf("python_cpci_read_bar1() : BAR1 0x%8.8x: 0x%8.8X (%c%c%c%c) PYTHON\n", 
         regNum , idval,
         (idval >> 24) & 0xFF,
         (idval >> 16) & 0xFF,
         (idval >> 8) & 0xFF,
         idval & 0xFF);

  return Py_BuildValue( "I" , idval );

}

static PyObject *python_cpci_write_bar1( PyObject *self , PyObject *args ) {

  int          regNum = 0;
  unsigned int  value = 0;
  if ( !PyArg_ParseTuple( args , "iI" , &(regNum) , &(value) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( regNum , value )" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  int idval = cpcitest_bar1_write( &dev, regNum , value );
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  return Py_BuildValue( "i" , idval );

}

// -- START OF TISC CODE 

static PyObject *python_cpci_read_glitz( PyObject *self , PyObject *args ) {

  unsigned int  glitz_bus = 0;
  unsigned int       addr = 0;
  if ( !PyArg_ParseTuple( args , "II" , &(glitz_bus) , &(addr) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( glitz_bus , regNum )" );
     return NULL;
  }
                                                                        /* bit   20  : flag to indicates glitz bus write              */
  unsigned int glitz_addr   = ( (glitz_bus&0x3) << 16 );                /* bit 17-16 : artix7 to write (0 ... 3)         (06/06/2014) */
  unsigned int glitz_offset = ( (addr&0x3fff)   << 2 );                 /* bit 15-02 : register in glitz to write        (06/06/2014) */
  unsigned int regNum_raw   = ( 0x100000 | glitz_addr | glitz_offset );
  unsigned int regNum       = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_read_glitz() : bus %d - addr = 0x%4.4x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x\n",
             glitz_bus , addr , regNum , regNum_raw );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( glitz_bus > 3 ) {
     PyErr_SetString( PyExc_ValueError , "glitz bus out of range" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_read( &dev, regNum );
  if ( debug_output ) {
     printf("CPCI Testing: GLITZ %d@0x%8.8x: 0x%8.8X (%c%c%c%c) [%u]\n",
   	 glitz_bus , addr ,
         idval,
         (idval >> 24) & 0xFF,
         (idval >> 16) & 0xFF,
         (idval >>  8) & 0xFF,
         idval & 0xFF ,
         idval );
  }

  return Py_BuildValue( "I" , idval );

}

static PyObject *python_cpci_write_glitz( PyObject *self , PyObject *args ) {

  unsigned int  glitz_bus = 0;
  unsigned int       addr = 0;
  unsigned int      value = 0;
  if ( !PyArg_ParseTuple( args , "III" , &(glitz_bus) , &(addr) , &(value) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( glitz_bus , regNum , value )" );
     return NULL;
  }
                                                                        /* bit   20  : flag to indicates glitz bus write              */
  unsigned int glitz_addr   = ( (glitz_bus&0x3) << 16 );                /* bit 17-16 : artix7 to write (0 ... 3)         (06/06/2014) */
  unsigned int glitz_offset = ( (addr&0x3fff)    << 2 );                /* bit 15-02 : register in glitz to write        (06/06/2014) */
  unsigned int regNum_raw   = ( 0x100000 | glitz_addr | glitz_offset );
  unsigned int regNum       = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_write use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_write_glitz() : bus %d - addr = 0x%4.4x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - value = 0x%8.8x [%u]\n",
             glitz_bus , addr , regNum , regNum_raw , value , value );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( glitz_bus > 3 ) {
     PyErr_SetString( PyExc_ValueError , "glitz bus out of range" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  int idval = cpcitest_bar1_write( &dev, regNum , value ); /* idval set to success code */
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  return Py_BuildValue( "i" , idval );

}

// -- START OF LONG CODE

static PyObject *python_cpci_read_long_artix( PyObject *self , PyObject *args ) {

  unsigned int  glitz_bus = 0;
  unsigned int       addr = 0;
  if ( !PyArg_ParseTuple( args , "II" , &(glitz_bus) , &(addr) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( glitz_bus , regNum )" );
     return NULL;
  }

                                                                        /* bit   20  : flag to indicates glitz bus write */
  unsigned int glitz_addr   = ( (glitz_bus&0x1) << 16 );                /* bit 17-16 : artix7 to write (0 ... 1)         */
  unsigned int glitz_offset = ( (addr&0x3fff)   << 2 );                 /* bit 15-02 : register in glitz to write        */
  unsigned int regNum_raw   = ( 0x100000 | glitz_addr | glitz_offset );
  unsigned int regNum       = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_read_long_artix() : bus %d - addr = 0x%4.4x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x\n",
             glitz_bus , addr , regNum , regNum_raw );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( glitz_bus < 0 ) || ( glitz_bus > 1 ) ) { 
     PyErr_SetString( PyExc_ValueError , "long-artix glitz bus out of range" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_read( &dev, regNum );
  if ( debug_output ) {
     printf("CPCI Testing: LONG_ARTIX %d@0x%8.8x: 0x%8.8X (%c%c%c%c) [%u]\n",
   	 glitz_bus , addr ,
         idval,
         (idval >> 24) & 0xFF,
         (idval >> 16) & 0xFF,
         (idval >>  8) & 0xFF,
         idval & 0xFF ,
         idval );
  }

  return Py_BuildValue( "I" , idval );

}

static PyObject *python_cpci_write_long_artix( PyObject *self , PyObject *args ) {

  unsigned int  glitz_bus = 0;
  unsigned int       addr = 0;
  unsigned int      value = 0;
  if ( !PyArg_ParseTuple( args , "III" , &(glitz_bus) , &(addr) , &(value) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( glitz_bus , regNum , value )" );
     return NULL;
  }
                                                                        /* bit   20  : flag to indicates glitz bus write */
  unsigned int glitz_addr   = ( (glitz_bus&0x1) << 16 );                /* bit 17-16 : artix7 to write (0 ... 1)         */
  unsigned int glitz_offset = ( (addr&0x3fff)   << 2 );                 /* bit 15-02 : register in glitz to write        */
  unsigned int regNum_raw   = ( 0x100000 | glitz_addr | glitz_offset );
  unsigned int regNum       = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_write use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_write_long_artix() : bus %d - addr = 0x%4.4x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - value = 0x%8.8x [%u]\n",
             glitz_bus , addr , regNum , regNum_raw , value , value );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( glitz_bus < 0 ) || ( glitz_bus > 1 ) ) { 
     PyErr_SetString( PyExc_ValueError , "long-artix glitz bus out of range" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  int idval = cpcitest_bar1_write( &dev, regNum , value ); /* idval set to success code */
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  return Py_BuildValue( "i" , idval );

}


static PyObject *python_cpci_set_long_dac( PyObject *self , PyObject *args ) {

  unsigned int    glitz_bus = 0;
  unsigned int dac_chain_no = 0;
  unsigned int  dac_chip_no = 0;
  unsigned int   dac_pin_no = 0;
  unsigned int    dac_value = 0;
  if ( !PyArg_ParseTuple( args , "IIIII" , &(glitz_bus) , &(dac_chain_no) , &(dac_chip_no) , &(dac_pin_no) , &(dac_value) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( glitz_bus , dac_chain_no , dac_chip_no , dac_pin_no , dac_value )" );
     return NULL;
  }
                                                                        /* bit   20  : flag to indicates glitz bus write */
  unsigned int glitz_addr      = ( (glitz_bus&0x1)    << 16 );          /* bit 17-16 : artix7 to write (0 ... 1)         */
  unsigned int dac_chain_addr  = ( (dac_chain_no&0x1) <<  8 );          /* bit 08    : DAC chain (0 ... 1)               */
  unsigned int dac_chip_addr   = ( (dac_chip_no&0x7)  <<  5 );          /* bit 07-05 : DAC chip number (0 ... 5)         */
  unsigned int dac_number_addr = ( (dac_pin_no&0x7)   <<  2 );          /* bit 04-02 : DAC number within chip (0 ... 7)  */
  unsigned int data_field      = ( dac_value & 0xFFFF );                /* DAC values are restricted to 16 bits          */
  unsigned int regNum_raw      = ( 0x100000 | glitz_addr | dac_chain_addr | dac_chip_addr | dac_number_addr );
  unsigned int regNum          = ( regNum_raw >> 2 );                   /* divide by 4 to account for bar1_write use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_set_long_dac() : bus %d - dac chain %d / chip %d / pin %d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - dac_value = 0x%8.8x - dac_field = 0x%8.8x [%u]\n",
             glitz_bus , dac_chain_no , dac_chip_no , dac_pin_no , regNum , regNum_raw , dac_value , data_field , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( glitz_bus < 0 ) || ( glitz_bus > 1 ) ) { 
     PyErr_SetString( PyExc_ValueError , "long-artix glitz bus out of range" );
     return NULL;
  }
  if ( ( dac_chain_no < 0 ) || ( dac_chain_no > 1 ) ) {
     PyErr_SetString( PyExc_ValueError , "long dac chain number out of range (0 ... 1)" );
     return NULL;
  }
  if ( ( dac_chip_no < 0 ) || ( dac_chip_no > 5 ) ) {
     PyErr_SetString( PyExc_ValueError , "long dac chip number out of range (0 ... 5)" );
     return NULL;
  }
  if ( ( dac_pin_no < 0 ) || ( dac_pin_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "long dac number out of range (0 ... 7)" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  int idval = cpcitest_bar1_write( &dev, regNum , data_field ); /* idval set to success code */
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  return Py_BuildValue( "i" , idval );

}


static PyObject *python_cpci_set_long_dac_by_channel( PyObject *self , PyObject *args ) {

  unsigned int   glitz_bus = 0;
  unsigned int polarity_no = 0;
  unsigned int  channel_no = 0;
  unsigned int dac_value = 0;
  if ( !PyArg_ParseTuple( args , "IIII" , &(glitz_bus) , &(polarity_no) , &(channel_no) , &(dac_value) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( glitz_bus , polarity_no , channel_no , value )" );
     return NULL;
  }

  if ( ( glitz_bus < 0 ) || ( glitz_bus > 1 ) ) { 
     PyErr_SetString( PyExc_ValueError , "long-artix glitz bus out of range" );
     return NULL;
  }
  if ( ( polarity_no < 0 ) || ( polarity_no > 1 ) ) { 
     PyErr_SetString( PyExc_ValueError , "long-artix polarity number out of range" );
     return NULL;
  }
  if ( ( channel_no < 0 ) || ( channel_no > 48 ) ) { 
     PyErr_SetString( PyExc_ValueError , "long-artix channel number out of range" );
     return NULL;
  }

  unsigned int  dac_chip_no = (channel_no >> 2); /* multiply by 2, then divide by 8, integer truncate */
  unsigned int dac_chain_no = 0;
  if ( dac_chip_no > 5 ) { /* chip_no above goes from 0 to 11 ... 0 to 5 are chain 0 and 6 to 11 are chain 1 */
     dac_chain_no = 1;
     dac_chip_no  -= 6;
  }
  polarity_no = polarity_no ? 0 : 1;
  unsigned int   dac_pin_no = (channel_no&0x3) + polarity_no;
                                                                        /* bit   20  : flag to indicates glitz bus write */
  unsigned int glitz_addr      = ( (glitz_bus&0x1)    << 16 );          /* bit 17-16 : artix7 to write (0 ... 1)         */
  unsigned int dac_chain_addr  = ( (dac_chain_no&0x1) <<  8 );          /* bit 08    : DAC chain (0 ... 1)               */
  unsigned int dac_chip_addr   = ( (dac_chip_no&0x7)  <<  5 );          /* bit 07-05 : DAC chip number (0 ... 5)         */
  unsigned int dac_number_addr = ( (dac_pin_no&0x7)   <<  2 );          /* bit 04-02 : DAC number within chip (0 ... 7)  */
  unsigned int data_field      = ( dac_value & 0xFFFF );                /* DAC values are restricted to 16 bits          */
  unsigned int regNum_raw      = ( 0x100000 | glitz_addr | dac_chain_addr | dac_chip_addr | dac_number_addr );
  unsigned int regNum          = ( regNum_raw >> 2 );                   /* divide by 4 to account for bar1_write use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_set_long_dac_by_channel() : bus %d - dac chain %d / chip %d / pin %d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - dac_value = 0x%8.8x - dac_field = 0x%8.8x [%u]\n",
             glitz_bus , dac_chain_no , dac_chip_no , dac_pin_no , regNum , regNum_raw , dac_value , data_field , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }

  if ( ( dac_chain_no < 0 ) || ( dac_chain_no > 1 ) ) {
     PyErr_SetString( PyExc_ValueError , "long dac chain number out of range (0 ... 1)" );
     return NULL;
  }
  if ( ( dac_chip_no < 0 ) || ( dac_chip_no > 5 ) ) {
     PyErr_SetString( PyExc_ValueError , "long dac chip number out of range (0 ... 5)" );
     return NULL;
  }
  if ( ( dac_pin_no < 0 ) || ( dac_pin_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "long dac number out of range (0 ... 7)" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  int idval = cpcitest_bar1_write( &dev, regNum , data_field ); /* idval set to success code */
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  return Py_BuildValue( "i" , idval );

}


static PyObject *python_cpci_read_long_artix_scalers( PyObject *self , PyObject *args ) {

  unsigned int    glitz_bus = 0;
  if ( !PyArg_ParseTuple( args , "I" , &(glitz_bus) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( glitz_bus )" );
     return NULL;
  }

  if ( ( glitz_bus < 0 ) || ( glitz_bus > 1 ) ) { 
     PyErr_SetString( PyExc_ValueError , "long-artix glitz bus out of range" );
     return NULL;
  }
    
  PyObject *new_tuple = PyTuple_New( 96 );
  if ( new_tuple == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory for tuple to store Artix LONG scalers" );
     return NULL;
  }

  unsigned int scaler_number = 0;
  unsigned int  dac_chain_no = 0;
  unsigned int   dac_chip_no = 0;
  unsigned int    dac_pin_no = 0;
  for( dac_chain_no = 0 ; dac_chain_no < 2 ; dac_chain_no++ ) {
     for( dac_chip_no = 0 ; dac_chip_no < 6 ; dac_chip_no++ ) {
        for( dac_pin_no = 0 ; dac_pin_no < 8 ; dac_pin_no++ ) {
                                                                                 /* bit   20  : flag to indicates glitz bus transaction */
	   unsigned int glitz_addr      = ( (glitz_bus&0x1)    << 16 );          /* bit 17-16 : artix7 to write (0 ... 1)               */
	   unsigned int scaler_addr     = 0xC000;                                /* bit 15-14 : '11' to indicate scaler bank            */
	   unsigned int dac_chain_addr  = ( (dac_chain_no&0x1) << 13 );          /* bit 13    : DAC chain (0 ... 1)                     */
	   unsigned int dac_chip_addr   = ( (dac_chip_no&0x7)  << 5 );           /* bit 07-05 : DAC chip number (0 ... 5)               */
	   unsigned int dac_pin_addr    = ( (dac_pin_no&0x7)   << 2 );           /* bit 04-02 : DAC number within chip (0 ... 7)        */
	   unsigned int regNum_raw      = ( 0x100000 | glitz_addr | scaler_addr | dac_chain_addr | dac_chip_addr | dac_pin_addr );
	   unsigned int regNum          = ( regNum_raw >> 2 );                   /* divide by 4 to account for bar1_write use of int32 pointer math */
#if 0
	   if ( debug_output ) {
	      printf( "python_cpci_read_long_artix_scalers() : bus %d - dac chain %d / chip %d / pin %d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x\n",
		      glitz_bus , dac_chain_no , dac_chip_no , dac_pin_no , regNum , regNum_raw );
	      fflush(stdout);
	   }
#endif
	   
	   if ( !dev.valid ) {
	      PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
	      return NULL;
	   }

	   if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
	      PyErr_SetString( PyExc_ValueError , "register number out of range" );
	      return NULL;
	   }

	   unsigned int idval = cpcitest_bar1_read( &dev, regNum ); /* idval set to success code */
	   if ( debug_output ) {
	      printf( "python_cpci_read_long_artix_scalers() : bus %d - scaler %2d - dac chain %d / chip %d / pin %d - regNum_raw = 0x%8.8x - value = 0x%4.4x [%u]\n",
		      glitz_bus , scaler_number , dac_chain_no , dac_chip_no , dac_pin_no , regNum_raw , idval , idval );
	      fflush(stdout);
	   }

	   if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
	      PyErr_SetString( PyExc_IOError , "return from cpci_bar1_read() : invalid handle - uic_generic_pci device not open" );
	      return NULL;
	   } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
	      PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_read() : register number out of range" );
	      return NULL;
	   }

           PyTuple_SetItem( new_tuple , scaler_number , Py_BuildValue( "I" , idval ) );  scaler_number++;

	} // end of FOR-loop over the 8 pins
     } // end of FOR-loop over the 6 DAC chips
  } // end of FOR-loop over the 2 DAC chains

  return new_tuple;

}


// -- START OF SURF CODE 

static PyObject *python_cpci_reboot_ice( PyObject *self , PyObject *args ) {

  int            ice_no = 0;
  unsigned int spi_bank = 0;
  if ( !PyArg_ParseTuple( args , "iI" , &(ice_no) , &(spi_bank) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( ice_no , spi_bank )" );
     return NULL;
  }

  unsigned int spi_addr_region = 0x0C0020;                /* ice40 spi flash address region on wishbone   */

  unsigned int ice_addr_field = 0;                        /* ice (i.e. LAB) number          - data[15:12] */
  if ( ice_no < 0 ) {
     ice_addr_field = ( ( 0xF ) << 12 );                  /* 0xF == broadcast to all ice                  */
  } else {
     ice_addr_field = ( ( ice_no&0xF ) << 12 );
  }
  unsigned int spi_bank_field = ( spi_bank & 0x7 );       /* spi bank                       - data[7:0]   */

  unsigned int     data_field = ( ice_addr_field | spi_bank_field );
  unsigned int     regNum_raw = spi_addr_region;
  unsigned int         regNum = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     if ( ice_no < 0 ) {
        printf( "python_cpci_reboot_ice() : LAB ALL - reboot to spi_bank %1d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	        spi_bank , regNum , regNum_raw , data_field );
     } else {
        printf( "python_cpci_reboot_ice() : LAB %3d - reboot to spi_bank %1d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	        ice_no , spi_bank , regNum , regNum_raw , data_field );
     }
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( ice_no > 11 ) && ( ice_no != -1 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid ICE/LAB number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  sleep( 1 );

  return Py_BuildValue( "I" , idval );

}


static PyObject *python_cpci_read_surf_i2c( PyObject *self , PyObject *args ) {

  int           lab_no = 0;
  unsigned int  dac_no = 0;
  if ( !PyArg_ParseTuple( args , "iI" , &(lab_no) , &(dac_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , dac_no )" );
     return NULL;
  }

  unsigned int i2c_addr_region = 0x080100;                /* i2c address region on wishbone               */

  unsigned int      update_bit = 0x100000;                /* write bit (required)           - data[20]    */
  unsigned int  lab_addr_field = 0;                       /* lab number                     - data[19:16] */
  if ( lab_no < 0 ) {
     lab_addr_field = ( ( 0xF ) << 16 );                  /* 0xF == broadcast to all ice                  */
  } else {
     lab_addr_field = ( (lab_no&0xF) << 16 );
  }
  unsigned int  dac_addr_field = ( (dac_no&0xF) << 12 );  /* dac number                     - data[15:12] */
  unsigned int      data_value = 0;                       /* data (set to 0 for read)       - data[11:0]  */

  unsigned int      data_field = ( update_bit | i2c_addr_region | lab_addr_field | dac_addr_field | data_value );
  unsigned int      regNum_raw = i2c_addr_region;
  unsigned int          regNum = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     if ( lab_no < 0 ) {
        printf( "python_cpci_read_surf_i2c() : LAB ALL - I2C DAC no = 0x%2.2x [%d]- regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	        dac_no , dac_no , regNum , regNum_raw , data_field );
     } else {
        printf( "python_cpci_read_surf_i2c() : LAB %2.2d - I2C DAC no = 0x%2.2x [%d]- regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
        	lab_no , dac_no , dac_no , regNum , regNum_raw , data_field );
     }
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( lab_no > 11 ) || ( lab_no != -1 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int write_idval = cpcitest_bar1_write( &dev, regNum , data_field ); /* do the I2C read request */
  if ( write_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( write_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_read( &dev, regNum );                     /* read back the response  */
  if ( debug_output ) {
     printf("CPCI Testing: LAB %2.2d @ I2C DAC 0x%2.2x : 0x%8.8x (%c%c%c%c) [%u]\n",
	    lab_no , dac_no ,
	    idval ,
	    (idval >> 24) & 0xFF ,
	    (idval >> 16) & 0xFF ,
	    (idval >>  8) & 0xFF ,
	    (idval & 0xFF )      ,
	    idval );
  }

  return Py_BuildValue( "I" , (idval&0x3FF) );

}

static PyObject *python_cpci_write_surf_i2c( PyObject *self , PyObject *args ) {

  int           lab_no = 0;
  unsigned int  dac_no = 0;
  unsigned int    data = 0;
  if ( !PyArg_ParseTuple( args , "iII" , &(lab_no) , &(dac_no) , &(data) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , dac_no , data )" );
     return NULL;
  }

  unsigned int i2c_addr_region = 0x0C0400;                /* i2c address region on wishbone               */

  unsigned int      update_bit = 0x100000;                /* write bit (required)           - data[20]    */
  unsigned int  lab_addr_field = 0;                       /* lab number                     - data[19:16] */
  if ( lab_no < 0 ) {
     lab_addr_field = ( ( 0xF ) << 16 );                  /* 0xF == broadcast to all ice                  */
  } else {
     lab_addr_field = ( (lab_no&0xF) << 16 );
  }
  unsigned int  dac_addr_field = ( (dac_no&0xF) << 12 );  /* dac number                     - data[15:12] */
  unsigned int      data_value = data & 0xFFF;            /* data                           - data[11:0]  */

  unsigned int      data_field = ( update_bit | lab_addr_field | dac_addr_field | data_value );
  unsigned int      regNum_raw = i2c_addr_region;
  unsigned int          regNum = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     if ( lab_no < 0 ) {
        printf( "python_cpci_write_surf_i2c() : LAB ALL - I2C DAC no = 0x%4.4x [%d]- regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x <- data = 0x%8.8x\n",
                dac_no , dac_no , regNum , regNum_raw , data_field , data );
     } else {
        printf( "python_cpci_write_surf_i2c() : LAB %d - I2C DAC no = 0x%4.4x [%d]- regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x <- data = 0x%8.8x\n",
                lab_no , dac_no , dac_no , regNum , regNum_raw , data_field , data );
     }
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( lab_no > 11 ) && ( lab_no != -1 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  sleep( 1.0 );

  return Py_BuildValue( "I" , idval );

}


static PyObject *python_cpci_write_labint_dac( PyObject *self , PyObject *args ) {

  int           lab_no = 0;
  unsigned int  dac_no = 0;
  unsigned int    data = 0;
  if ( !PyArg_ParseTuple( args , "iII" , &(lab_no) , &(dac_no) , &(data) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , dac_no , data )" );
     return NULL;
  }

  unsigned int lab_dac_addr_region = 0x0C0024;                /* ice40 LAB DAC load address region on wishbone */

  unsigned int lab_addr_field = 0;                            /* ice (i.e. LAB) number          - data[15:12]  */
  if ( lab_no < 0 ) {
     lab_addr_field = ( ( 0xF ) << 12 );                      /* 0xF == broadcast to all ice                   */
  } else {
     lab_addr_field = ( ( lab_no&0xF ) << 12 );
  }
  unsigned int dac_addr_field = ( dac_no & 0xFFF );           /* DAC address                    - data[11:0]   */
  unsigned int dac_data_field = (   data & 0xFFF ) << 20;     /* DAC data value to write        - data[31:20]  */

  unsigned int     data_field = ( lab_addr_field | dac_addr_field | dac_data_field );
  unsigned int     regNum_raw = lab_dac_addr_region;
  unsigned int         regNum = ( regNum_raw >> 2 );                      /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     if ( lab_no < 0 ) {
        printf( "python_cpci_write_labint_dac() : LAB ALL - write 0x%4.4x to internal DAC %d of all LABs - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	         (data&0xFFF) , (dac_no&0xFFF) , regNum , regNum_raw , data_field );
     } else {
        printf( "python_cpci_write_labint_dac() : LAB %3d - write 0x%4.4x to internal DAC %d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	         lab_no , (data&0xFFF) , (dac_no&0xFFF) , regNum , regNum_raw , data_field );
     }
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( lab_no > 11 ) && ( lab_no != -1 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  sleep( 1.0 );

  return Py_BuildValue( "I" , idval );

}


static PyObject *python_cpci_write_wraddr_phase( PyObject *self , PyObject *args ) {

  int           lab_no = 0;
  unsigned int    data = 0;
  if ( !PyArg_ParseTuple( args , "iI" , &(lab_no) , &(data) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , data )" );
     return NULL;
  }

  unsigned int lab_wraddr_phase_register = 0x0C0008;         /* ice40 LAB WRADDR_PHASE load address on wishbone */

  unsigned int lab_addr_field = 0;                           /* ice (i.e. LAB) number          - data[15:12]    */
  if ( lab_no < 0 ) {
     lab_addr_field = ( ( 0xF ) << 12 );                     /* 0xF == broadcast to all ice                     */
  } else {
     lab_addr_field = ( ( lab_no&0xF ) << 12 );
  }
  unsigned int phase_data_field = ( data & 0x1F );           /* phase setting to write         - data[04:00]    */

  unsigned int       data_field = ( lab_addr_field | phase_data_field );
  unsigned int       regNum_raw = lab_wraddr_phase_register;
  unsigned int           regNum = ( regNum_raw >> 2 );                   /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     if ( lab_no < 0 ) {
        printf( "python_cpci_write_wraddr_phase() : LAB ALL - write 0x%4.4x to write address phase register - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	         data , regNum , regNum_raw , data_field );
     } else {
        printf( "python_cpci_write_wraddr_phase() : LAB %3d - write 0x%4.4x to write address phase register - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	         lab_no , data , regNum , regNum_raw , data_field );
     }
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( lab_no > 11 ) && ( lab_no != -1 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  sleep( 1.0 );

  return Py_BuildValue( "I" , idval );

}


//xxxxxx

static PyObject *python_cpci_write_lab4c_dTs( PyObject *self , PyObject *args ) {

  unsigned int      trace_debug = 0;       /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  unsigned int lab_dac_addr_region = 0x0C0024;                /* ice40 LAB DAC load address region on wishbone */

  unsigned int  lab_no = 0;
  PyObject     *tuple_of_delay_line_tap_values = NULL;
  if ( !PyArg_ParseTuple( args , "IO" , &(lab_no) , &(tuple_of_delay_line_tap_values) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , tuple_of_delay_line_tap_values )" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number" );
     return NULL;
  }

  unsigned int tuple_size = PyTuple_Size( tuple_of_delay_line_tap_values );
  if ( tuple_size != 127 ) {
     PyErr_SetString( PyExc_ValueError , "tuple_of_delay_line_tap_values must have 127 entries" );
     return NULL;
  }

  unsigned int *tuple_values = (unsigned int *) malloc( tuple_size*sizeof(unsigned int) ); // allocate memory to temporarily store values
  if ( tuple_values == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory to temporarily store tuple_values for dT write" );
     return NULL;
  }

  unsigned int ii=0;
  for( ii=0 ; ii<tuple_size ; ii++ ) {
     PyObject       *value_pyobject = PyTuple_GetItem( tuple_of_delay_line_tap_values , ii );
     unsigned long         value_ul = PyInt_AsUnsignedLongMask( value_pyobject );
     tuple_values[ii] = ( value_ul & 0xffff );
     if ( trace_debug ) {
        printf( "python_cpci_write_lab4c_dTs( ) : input_tuple[%d] = 0x%8.8lx -> tuple_values[%d] = 0x%8.8x" , 
		ii , value_ul ,
		ii , tuple_values[ii] );
	fflush(stdout);
     }
  }

  unsigned int number_of_dacs_written = 0;
  unsigned int dt_dac_no = 0;
  for( dt_dac_no=0 ; dt_dac_no<tuple_size ; dt_dac_no++ ) {

     unsigned int           data = tuple_values[dt_dac_no];

     unsigned int lab_addr_field = ( ( lab_no&0xF ) << 12 );     /* ice (i.e. LAB) number          - data[15:12]  */
     unsigned int dac_addr_field = ( dt_dac_no & 0xFFF );        /* DAC address                    - data[11:0]   */
     unsigned int dac_data_field = (   data & 0xFFF ) << 20;     /* DAC data value to write        - data[31:20]  */

     unsigned int     data_field = ( lab_addr_field | dac_addr_field | dac_data_field );
     unsigned int     regNum_raw = lab_dac_addr_region;
     unsigned int         regNum = ( regNum_raw >> 2 );          /* divide by 4 to account for bar1_read use of int32 pointer math */
     if ( debug_output ) {
        printf( "python_cpci_write_lab4c_dTs( ) : LAB %3d - write dT DAC %3d - value = 0x%4.4x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
		lab_no , dt_dac_no , (data&0xFFF) , regNum , regNum_raw , data_field );
     }

     if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
        PyErr_SetString( PyExc_ValueError , "register number out of range" );
	if ( tuple_values != NULL ) {
	   free( tuple_values );
	   tuple_values = NULL;
	}
	return NULL;
     }

     unsigned int idval = cpcitest_bar1_write( &dev, regNum , data_field );
     if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
        PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
	if ( tuple_values != NULL ) {
	   free( tuple_values );
	   tuple_values = NULL;
	}
	return NULL;
     } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
        PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
	if ( tuple_values != NULL ) {
	   free( tuple_values );
	   tuple_values = NULL;
	}
	return NULL;
     }

     usleep( 20 );  /* wait 20 useconds between writes to allow ICE40 to complete write to LAB4C */

     number_of_dacs_written++;

  }

  if ( tuple_values != NULL ) {
     free( tuple_values );
     tuple_values = NULL;
  }

  return Py_BuildValue( "I" , number_of_dacs_written );

}

// xxxxxx

int cpci_set_dt_mode( unsigned int lab_no , unsigned int dt_mode ) {

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     printf( "cpci_set_dt_mode( ) : invalid lab number - value = %d\n",lab_no );
     return -1;
  }

  unsigned int    ice40_general_register = 0x0C0028;                        /* GENERAL COMMENT register for ICE40          */

  unsigned int data_value = 0x00;
  if ( dt_mode == 0 ) {
     printf( "cpci_set_dt_mode( ) : lab_no = %d - dt_mode = \"Common VdlyN\" [%d]\n",
	     lab_no , dt_mode );
     data_value = 0x00;
  } else if ( dt_mode == 1 ) {
     printf( "cpci_set_dt_mode( ) : lab_no = %d - dt_mode = \"Individual Tap VdlyN\" [%d]\n",
	     lab_no , dt_mode );
     data_value = 0x02;
  } else {
     return -3; /* INVALID dT MODE CHOICE */
  }

  /*          A : execute the command into the general command register                                                     */

  unsigned int   lab_addr_field = ( (lab_no&0xf) << 12 );                    /* lab addr                      - data[15:12] */
  unsigned int data_value_field = ( data_value & 0xff );                     /* data_value                    - data[7:0]   */
  unsigned int       data_field = lab_addr_field | data_value_field;
  unsigned int       regNum_raw = ice40_general_register;
  unsigned int           regNum = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  if ( debug_output ) {
     printf( "cpci_set_dt_mode( ) : - write 0x%8.8x to \"ICE40 GENERAL COMMAND\" register to program - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
	     data_value , regNum , regNum_raw , data_field );
  }

  int idval = cpcitest_bar1_write( &dev, regNum , data_field );

  return idval;

}


static PyObject *python_cpci_set_dt_mode( PyObject *self , PyObject *args ) {

  unsigned int  lab_no  = 0;
  unsigned int  dt_mode_flag = 0; /* =0, common dT mode ... =1, individual dT mode */
  if ( !PyArg_ParseTuple( args , "II" , &(lab_no) , &(dt_mode_flag) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , dt_mode_flag )" );
    return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for executing a SPI memory function at this time" );
     return NULL;
  }

  if ( ( dt_mode_flag < 0 ) || ( dt_mode_flag > 1 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid dT mode flag (=0, common dT mode/=1, individual dT mode)" );
     return NULL;
  }

  printf( "python_cpci_set_dt_mode( ) : lab_no = %d - set dT mode flag = %d\n",
	  lab_no , dt_mode_flag );

  int return_code = cpci_set_dt_mode( lab_no , dt_mode_flag );

  if ( return_code != 0 ) {
     return NULL;   /* no need to state reason here as routine does that before exitting */
  }

  return Py_BuildValue( "I" , return_code );

}

//yyyyyxxxx


static PyObject *python_cpci_get_firmware_id( PyObject *self , PyObject *args ) {

  int           lab_no = 0;
  if ( !PyArg_ParseTuple( args , "i" , &(lab_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no )" );
     return NULL;
  }

  unsigned int lab_firmware_id_addr_region = 0x0C002C;                /* ice40 LAB firmware id address command on wishbone */

  unsigned int lab_addr_field = ( ( lab_no&0xF ) << 12 );             /* ice (i.e. LAB) number          - data[15:12]      */

  unsigned int     data_field = ( lab_addr_field );
  unsigned int     regNum_raw = lab_firmware_id_addr_region;
  unsigned int         regNum = ( regNum_raw >> 2 );                  /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_get_firmware_id() : LAB %3.3d - read from firmware id - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
   	     lab_no , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int write_idval = cpcitest_bar1_write( &dev, regNum , data_field ); /* do the LAB internal DAC read request */
  if ( write_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( write_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  if ( debug_output ) {
     printf( "CPCI Testing: sleep 1s after write\n" );
  }
  sleep( 1 );

  unsigned int idval               = cpcitest_bar1_read( &dev, regNum );   /* read back the response               */
  unsigned int firmwareid_readback = idval & 0xffff;                       /* firmware id            - data[15:0]  */

  if ( debug_output ) {
     printf( "CPCI Testing: LAB %2.2d @ firmware id check : 0x%8.8x (%c%c%c%c) [%u] - READBACK : firmware id = 0x%8.8x\n",
	     lab_no ,
	     idval ,
	     (idval >> 24) & 0xFF ,
	     (idval >> 16) & 0xFF ,
	     (idval >>  8) & 0xFF ,
	     ( idval & 0xFF ) ,
	     idval ,
	     firmwareid_readback );
  }

  return Py_BuildValue( "I" , idval );

}

static PyObject *python_cpci_initialize_surf_for_digitization( PyObject *self , PyObject *args ) {

  unsigned int  turf_or_pci_trigger_source_flag = 0;
  if ( !PyArg_ParseTuple( args , "i" , &(turf_or_pci_trigger_source_flag) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( turf_or_pci_trigger_source_flag )" );
     return NULL;
  }

  unsigned int turf_pci_addr_register = 0x0C003C;                        /* Artix7 : turf(0)/pci(1) trigger selection register  */

  unsigned int             data_field = turf_or_pci_trigger_source_flag; /*          only the least significant bit counts      */
  unsigned int             regNum_raw = turf_pci_addr_register;
  unsigned int                 regNum = ( regNum_raw >> 2 );             /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_initialize_surf_for_digitization() : - write 0x%4.4x to turf/pci trigger selection register - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
             data_field , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int write_idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( write_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( write_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  sleep( 1.0 );

  unsigned int readback_idval = cpcitest_bar1_read( &dev, regNum );
  printf( "python_cpci_initialize_surf_for_digitization() : - readback %d from turf/pci trigger source register - readback_idval = 0x%8.8x\n",
          ( readback_idval & 0x1 ) , 
          readback_idval );

  current_bank = 0; /* initialize current storage capacitor bank for all bank being written to bank 0 to start */

  return Py_BuildValue( "I" , readback_idval );

}

static PyObject *python_cpci_hold_lab_bank( PyObject *self , PyObject *args ) {

  unsigned int  bank_mask = 0;
  if ( !PyArg_ParseTuple( args , "I" , &(bank_mask) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( bank_mask )" );
     return NULL;
  }

  unsigned int hold_lab_bank_register = 0x0C0038;                /* hold bank register for all LABs               */

  unsigned int          data_field = ( bank_mask & 0xf );        /* bit 0: hold bank 0 / bit 1: hold bank 1 / bit 2: hold bank 2 / bit 3: hold bank 3 */
  unsigned int          regNum_raw = hold_lab_bank_register;
  unsigned int              regNum = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_hold_lab_bank() : - write 0x%8.8x to \"hold lab bank\" register to hold bank(s) 0x%1.1x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
             data_field , ( bank_mask&0xf ) , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

//sleep( 1.0 );

  unsigned int readback_idval = cpcitest_bar1_read( &dev, regNum );
  printf( "python_cpci_hold_lab_bank() : - readback %d from \"lab hold bank\" register - readback_idval = 0x%8.8x\n",
          ( readback_idval & 0x1 ) , 
          readback_idval );

  return Py_BuildValue( "I" , readback_idval );

}

static PyObject *python_cpci_digitize_lab_bank( PyObject *self , PyObject *args ) {

  unsigned int  bank_no = 0;
  if ( !PyArg_ParseTuple( args , "I" , &(bank_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( bank_no )" );
     return NULL;
  }

  unsigned int digitize_lab_bank_register = 0x0C0000;                   /* digitize lab bank register for all LABs       */
  unsigned int  ice40_spiexecute_register = 0x0C0034;                   /* SPI_EXECUTE register for ICE40                */

  /*           A : build the LAB4x digitize command                                                                      */

  unsigned int                 data_field = ( bank_no & 0x3 );          /* bank number: 0 to 3                           */
  unsigned int                 regNum_raw = digitize_lab_bank_register;
  unsigned int                     regNum = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_digitize_lab_bank() : - write 0x%8.8x to \"digitize lab bank\" register to hold bank %d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
             data_field , bank_no , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( bank_no < 0 ) || ( bank_no > 3 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid BANK number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  /*           B : issue the LAB4x digitize command                                                                      */

  unsigned int idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

#if 1
  /*           C : wait for the ICE40 to complete the operation (Luca bits)                                              */

  regNum_raw = ice40_spiexecute_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  int read_idval = cpcitest_bar1_read( &dev, regNum );
  int  ready_bit = read_idval & 0x2;                    /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  while ( ready_bit != 0 ) {
        read_idval = cpcitest_bar1_read( &dev, regNum );
	ready_bit  = read_idval & 0x2;                  /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  }
#else
  sleep( 1.0 );
#endif

  /*           D : check the "fresh LAB4x data bit" (Luca bits)                                                          */

  regNum_raw = digitize_lab_bank_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  unsigned int readback_idval = cpcitest_bar1_read( &dev, regNum );
  printf( "python_cpci_digitize_lab_bank() : - readback %d from \"digitize lab bank\" register - readback_idval = 0x%8.8x\n",
          ( readback_idval & 0x1 ) , 
          readback_idval );

  return Py_BuildValue( "I" , readback_idval );

}


static PyObject *python_cpci_full_digitization_of_lab( PyObject *self , PyObject *args ) {

  unsigned int  data_field = 0;
  unsigned int  regNum_raw = 0;
  unsigned int      regNum = 0;

  unsigned int  bank_mask = 0;
  unsigned int  bank_to_digitize = 0;
  if ( !PyArg_ParseTuple( args , "II" , &(bank_mask) , &(bank_to_digitize) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( bank_mask , bank_to_digitize )" );
     return NULL;
  }

  // - hold the bank

  unsigned int hold_lab_bank_register = 0x0C0038;  /* hold bank register for all LABs               */

  data_field = ( bank_mask & 0xf );                /* bit 0: hold bank 0 / bit 1: hold bank 1 / bit 2: hold bank 2 / bit 3: hold bank 3 */
  regNum_raw = hold_lab_bank_register;
  regNum     = ( regNum_raw >> 2 );                /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_hold_lab_bank() : - write 0x%8.8x to \"hold lab bank\" register to hold bank(s) 0x%1.1x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
             data_field , ( bank_mask&0xf ) , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int hold_idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( hold_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( hold_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  unsigned int readback_idval = cpcitest_bar1_read( &dev, regNum );
  printf( "python_cpci_hold_lab_bank() : - readback %d from \"lab hold bank\" register - readback_idval = 0x%8.8x\n",
          ( readback_idval & 0x1 ) , 
          readback_idval );

  // digitize the bank

  unsigned int digitize_lab_bank_register = 0x0C0000;                   /* digitize lab bank register for all LABs       */
  unsigned int  ice40_spiexecute_register = 0x0C0034;                   /* SPI_EXECUTE register for ICE40                */

  /*           A : build the LAB4x digitize command                                                                      */

  data_field = ( bank_to_digitize & 0x3 );          /* bank number: 0 to 3                           */
  regNum_raw = digitize_lab_bank_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_digitize_lab_bank() : - write 0x%8.8x to \"digitize lab bank\" register to hold bank %d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
             data_field , bank_to_digitize , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( bank_to_digitize < 0 ) || ( bank_to_digitize > 3 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid BANK number" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  /*           B : issue the LAB4x digitize command                                                                      */

  unsigned int digitize_idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( digitize_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( digitize_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

#if 1
  /*           C : wait for the ICE40 to complete the operation (Luca bits)                                              */

  regNum_raw = ice40_spiexecute_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  int ready_idval = cpcitest_bar1_read( &dev, regNum );
  int  ready_bit = ready_idval & 0x2;                    /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  while ( ready_bit != 0 ) {
        ready_idval = cpcitest_bar1_read( &dev, regNum );
	ready_bit  = ready_idval & 0x2;                  /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  }
#else
  sleep( 1.0 );
#endif

  /*           D : check the "fresh LAB4x data bit" (Luca bits)                                                          */

  regNum_raw = digitize_lab_bank_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  readback_idval = cpcitest_bar1_read( &dev, regNum );
  printf( "python_cpci_digitize_lab_bank() : - readback %d from \"digitize lab bank\" register - readback_idval = 0x%8.8x\n",
          ( readback_idval & 0x1 ) , 
          readback_idval );

  return Py_BuildValue( "I" , readback_idval );

}


static PyObject *python_cpci_digitize_all_lab_banks( PyObject *self , PyObject *args ) {

  unsigned int  data_field = 0;
  unsigned int  regNum_raw = 0;
  unsigned int      regNum = 0;

  // - hold all bank

  unsigned int hold_lab_bank_register = 0x0C0038;  /* hold bank register for all LABs               */

  data_field = ( 0xf );                            /* bit 0: hold bank 0 / bit 1: hold bank 1 / bit 2: hold bank 2 / bit 3: hold bank 3 */
  regNum_raw = hold_lab_bank_register;
  regNum     = ( regNum_raw >> 2 );                /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_digitize_all_lab_banks() : - write 0x%8.8x to \"hold lab bank\" register to hold bank(s) 0x%1.1x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
             data_field , ( 0xf ) , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  unsigned int hold_idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( hold_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( hold_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  unsigned int readback_idval = cpcitest_bar1_read( &dev, regNum );
  printf( "python_cpci_digitize_all_lab_banks() : - readback %d from \"lab hold bank\" register - readback_idval = 0x%8.8x\n",
          ( readback_idval & 0x1 ) , 
          readback_idval );

  // digitize all bank

  unsigned int digitize_lab_bank_register = 0x0C0000;                   /* digitize lab bank register for all LABs       */
  unsigned int  ice40_spiexecute_register = 0x0C0034;                   /* SPI_EXECUTE register for ICE40                */

  unsigned int return_code = 0;
  unsigned int bank_to_digitize = 0;
  for( bank_to_digitize=0 ; bank_to_digitize<4 ; bank_to_digitize++ ) {

  /*           A : build the LAB4x digitize command                                                                      */

     data_field = ( bank_to_digitize & 0x3 );          /* bank number: 0 to 3                           */
     regNum_raw = digitize_lab_bank_register;
     regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */
     if ( debug_output ) {
        printf( "python_cpci_digitize_all_lab_banks() : - write 0x%8.8x to \"digitize lab bank\" register to hold bank %d - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
		data_field , bank_to_digitize , regNum , regNum_raw , data_field );
     }

     if ( !dev.valid ) {
        PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
	return NULL;
     }
     if ( ( bank_to_digitize < 0 ) || ( bank_to_digitize > 3 ) ) {
        PyErr_SetString( PyExc_ValueError , "invalid BANK number" );
	return NULL;
     }
     if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
        PyErr_SetString( PyExc_ValueError , "register number out of range" );
	return NULL;
     }

  /*           B : issue the LAB4x digitize command                                                                      */

     unsigned int digitize_idval = cpcitest_bar1_write( &dev, regNum , data_field );
     if ( digitize_idval == CPCITEST_ERR_INVALID_HANDLE ) {
        PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
	return NULL;
     } else if ( digitize_idval == CPCITEST_ERR_INVALID_REGISTER ) {
        PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
	return NULL;
     }

#if 1
  /*           C : wait for the ICE40 to complete the operation (Luca bits)                                              */

     regNum_raw = ice40_spiexecute_register;
     regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

     int ready_idval = cpcitest_bar1_read( &dev, regNum );
     int  ready_bit = ready_idval & 0x2;                    /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
     while ( ready_bit != 0 ) {
           ready_idval = cpcitest_bar1_read( &dev, regNum );
	   ready_bit  = ready_idval & 0x2;                  /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
     }
#else
     sleep( 1.0 );
#endif

  /*           D : check the "fresh LAB4x data bit" (Luca bits)                                                          */

     regNum_raw = digitize_lab_bank_register;
     regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

     readback_idval = cpcitest_bar1_read( &dev, regNum );
     printf( "python_cpci_digitize_all_lab_banks() : - readback %d from \"digitize lab bank\" register - readback_idval = 0x%8.8x\n",
	     ( readback_idval & 0x1 ) , 
	     readback_idval );

     return_code = return_code | ( ( readback_idval & 0x1 ) << (bank_to_digitize) );

  } // end of FOR-loop over the 4 banks

  // - release all bank (next write will be in bank 0 as it should be following the readout of all 4 banks)

  data_field = ( 0x0 );                            /* bit 0: hold bank 0 / bit 1: hold bank 1 / bit 2: hold bank 2 / bit 3: hold bank 3 */
  regNum_raw = hold_lab_bank_register;
  regNum     = ( regNum_raw >> 2 );                /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_digitize_all_lab_banks() : - write 0x%8.8x to \"hold lab bank\" register to hold bank(s) 0x%1.1x - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
             data_field , ( 0x0 ) , regNum , regNum_raw , data_field );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( regNum < 0 || regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) {
     PyErr_SetString( PyExc_ValueError , "register number out of range" );
     return NULL;
  }

  hold_idval = cpcitest_bar1_write( &dev, regNum , data_field );
  if ( hold_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( hold_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  readback_idval = cpcitest_bar1_read( &dev, regNum );
  printf( "python_cpci_digitize_all_lab_banks() : - readback %d from \"lab hold bank\" register - readback_idval = 0x%8.8x\n",
          ( readback_idval & 0x1 ) , 
          readback_idval );

  return Py_BuildValue( "I" , return_code );

}


#define LAB4x_READOUT_SAMPLE_SIZE 1024  /* 8 windows of 128 samples = 1024 samples -- NOTE: this number must be an even number! */

static PyObject *python_cpci_fetch_lab_data( PyObject *self , PyObject *args ) {

  unsigned int   lab_no = 0;
  unsigned int  bank_no = 0;
  if ( !PyArg_ParseTuple( args , "II" , &(lab_no) , &(bank_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , bank_no )" );
     return NULL;
  }
                                                                        /* LAB data RAM addr key - 10AA AABb bwww ssss ss00              */

  unsigned int         lab_data_base_addr = 0x080000;                   /* base register address for start of lab data RAM - '10__'      */
//unsigned int          artix_bank_offset = 0;                          /* offset in lab data RAM for the ARTIX bank       - data[13]    */
  unsigned int                bank_offset = ( (bank_no&0x3) << 11 );    /* offset in lab data RAM for this bank            - data[12:11] */
  unsigned int                 lab_offset = ( ( lab_no&0xf) << 14 ) ;   /* offset in lab data RAM to this lab in the bank  - data[17:14] */

  unsigned int            base_regNum_raw = ( lab_data_base_addr | bank_offset | lab_offset ); /* may want to use addition here? */
  unsigned int                base_regNum = ( base_regNum_raw >> 2 );   /* divide by 4 to account for bar1_read use of int32 pointer math */
  if ( debug_output ) {
     printf( "python_cpci_fetch_lab_data() : - read lab %2.2d in bank %d - base_regNum = 0x%8.8x <- base_regNum_raw = 0x%8.8x\n",
             lab_no , bank_no , base_regNum , base_regNum_raw );
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }
  if ( ( bank_no < 0 ) || ( bank_no > 3 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid BANK number" );
     return NULL;
  }
  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number" );
     return NULL;
  }

  PyObject *new_tuple = PyTuple_New( LAB4x_READOUT_SAMPLE_SIZE );
  if ( new_tuple == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory for tuple to store LAB4x data read back from SPI memory" );
     return NULL;
  }

  int readout_count = 0;
  int   this_sample = 0;
  for( readout_count = 0 ; readout_count < (LAB4x_READOUT_SAMPLE_SIZE/2) ; readout_count++ ) {
     unsigned int regNum = base_regNum + readout_count;
     if ( ( regNum < 0 ) || ( regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) ) {
        PyErr_SetString( PyExc_ValueError , "register number out of range" );
	return NULL;
     }
     unsigned int idval = cpcitest_bar1_read( &dev, regNum );
     if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
        PyErr_SetString( PyExc_IOError , "return from cpci_bar1_read() : invalid handle - uic_generic_pci device not open" );
        return NULL;
     } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
        PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_read() : register number out of range" );
        return NULL;
     } 
     unsigned int sample_l = ( idval & 0x0000FFFF );         /* LSW of 32-bit data readout is sample_n   */
     unsigned int sample_m = ( idval & 0xFFFF0000 ) >> 16;   /* MSW of 32-bit data readout is sample_n+1 */
     printf( "python_cpci_fetch_lab_data() : - readback sample %4.4d - register = 0x%4.4x - data = 0x%8.8x - sample_l = 0x%4.4x (%-4u) / sample_m = 0x%4.4x (%-4u)\n",
	     readout_count ,
             regNum        , 
             idval         ,
             sample_l , sample_l , sample_m , sample_m );
     PyTuple_SetItem( new_tuple , this_sample , Py_BuildValue( "I" , sample_l ) );  this_sample++;
     PyTuple_SetItem( new_tuple , this_sample , Py_BuildValue( "I" , sample_m ) );  this_sample++;
  }

  return new_tuple;

}

static PyObject *python_cpci_load_ice40_firmware( PyObject *self , PyObject *args ) {

  char          *file_name = NULL;
  if ( !PyArg_ParseTuple( args , "s" , &(file_name) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( file_name )" );
     return NULL;
  }

  if ( file_name == NULL ) {
     PyErr_SetString( PyExc_ValueError , "file name with ICE40 firmware not specified" );
     return NULL;
  }     

  if ( ice40_firmware_buffer != NULL ) {                                                          // first free any allocated memory that might be storing an previous firmware file
     printf( "python_cpci_load_ice40_firmware( ) : -> found previous firmware in memory ... overwriting it\n" );
     fflush(stdout);
     free( ice40_firmware_buffer );
     ice40_firmware_buffer = NULL;
     ice40_firmware_buffer_size = 0;
  }
  if ( ice40_firmware_file_name != NULL ) {
     free( ice40_firmware_file_name );
     ice40_firmware_file_name = NULL;
  }

  // - read the firmware file into memory

  FILE *FP = fopen( file_name , "rb" );                  // open up the file
  if ( FP == NULL ) {
     PyErr_SetString( PyExc_IOError , "file with ICE40 firmware not found" );
     return NULL;
  }

  fseek( FP , 0 , SEEK_END );                            // obtain file size:
  long int file_size_from_seek = ftell( FP );
  rewind( FP );
  if ( file_size_from_seek == 0 ) {
     PyErr_SetString( PyExc_IOError , "read error - firmware file is empty" );
     fflush(stdout);
     fclose( FP );
     return NULL;
  }
  printf( "python_cpci_load_ice40_firmware( ) : -> openned file \"%s\" - seek file size = %ld bytes\n" , file_name , file_size_from_seek );
  fflush(stdout);

  ice40_firmware_buffer = (unsigned char *) malloc( sizeof(unsigned char)*file_size_from_seek );  // allocate memory to contain the whole file
  if ( ice40_firmware_buffer == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate buffer memory for ICE40 firmware" );
     fclose( FP );
     return NULL;
  }

  long int file_size_from_fread = fread( ice40_firmware_buffer , 1 , file_size_from_seek , FP );  // read the firmware into the buffer
  printf( "python_cpci_load_ice40_firmware( ) : ->    read file \"%s\" - read file size = %ld bytes\n" , file_name , file_size_from_fread );
  fflush(stdout);
  if ( file_size_from_fread != file_size_from_seek ) {
     PyErr_SetString( PyExc_IOError , "read error - read file size not equal to seek file size" );
     fflush(stdout);
     fclose( FP );
     if ( ice40_firmware_buffer != NULL ) {
        free( ice40_firmware_buffer );
	ice40_firmware_buffer = NULL;
	ice40_firmware_buffer_size = 0;
     }
     return NULL;
  }
  ice40_firmware_buffer_size = file_size_from_fread;

  fclose( FP );                                          // close the file
  printf( "python_cpci_load_ice40_firmware( ) : -> closing file \"%s\"\n" , file_name );
  fflush(stdout);

  ice40_firmware_file_name = malloc( sizeof(char) * (strlen(file_name)+1) ); // latch the file name for reference
  strcpy( ice40_firmware_file_name , file_name );

  return Py_BuildValue( "I" , ice40_firmware_buffer_size );

}


int cpci_extract_firmware_bank_from_firmware_image( unsigned firmware_bank_no ) {

  unsigned int      trace_debug = 0;       /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */
//unsigned int        page_size = 256;     /* number of bits in a single SPI memory page                                          */
  unsigned int        bank_size = 256*256; /* each bank contains 256 pages, each page being 256-bytes in size                     */

  if ( ice40_firmware_file_name != NULL ) {
     printf( "cpci_extract_firmware_bank_from_firmware_image( ) : current firmware file - \"%s\"\n",
	     ice40_firmware_file_name );
     fflush(stdout);
  }
  if ( ( ice40_firmware_buffer == NULL ) || ( ice40_firmware_buffer_size < 1 ) ) {
     printf( "cpci_extract_firmware_bank_from_firmware_image( ) : ERROR - ICE40 firmware not stored in memory\n" );
     fflush(stdout);
     return -1;
  }

  // - verify that the firmware image contains the specified bank before doing anything

  unsigned int         number_of_banks = ice40_firmware_buffer_size / bank_size;
  unsigned int       size_of_last_bank = ice40_firmware_buffer_size % bank_size;
  printf( "cpci_extract_firmware_bank_from_firmware_image( ) : -> firmware image : has %d banks - size of last incomplete bank = %d\n",
	  number_of_banks , size_of_last_bank );
  fflush(stdout);

  if ( number_of_banks < firmware_bank_no ) {
     printf( "cpci_extract_firmware_bank_from_firmware_image( ) : - firmware file is not that large\n" );
     fflush(stdout);
     return -2;
  }

  // - initialize the firmware image array

  if ( firmware_bank_image == NULL ) {
     printf( "cpci_extract_firmware_bank_from_firmware_image( ) : -> allocating memory for firmware_bank_image, size = %d\n" , bank_size );
     fflush(stdout);
     firmware_bank_image = (unsigned char *) malloc( bank_size*sizeof(unsigned char) );   // allocate memory to contain one page of the file
     if ( firmware_bank_image == NULL ) {
        printf( "cpci_extract_firmware_bank_from_firmware_image( ) : ERROR - unable to allocate memory for firmware_bank_image\n" );
	fflush(stdout);
	return -3;
     }
     firmware_bank_image_size = 0;
  } else {
     printf( "cpci_extract_firmware_bank_from_firmware_image( ) : -> replacing current firmware bank image, was bank %d - will be bank %d\n" ,
             firmware_bank_image_bank_no , firmware_bank_no );
     fflush(stdout);
  }

  printf( "cpci_extract_firmware_bank_from_firmware_image( ) : -> zeroing out firmware_bank_image\n" );
  fflush(stdout);

  memset( firmware_bank_image , 0 , bank_size );                                          // -> zero the page memory to avoid any carryover from previous page
  firmware_bank_image_size = 0;

  unsigned int   remaining_bytes_to_write = bank_size;
  unsigned int  current_firmware_position = firmware_bank_no * bank_size;
  
  printf( "cpci_extract_firmware_bank_from_firmware_image( ) : -> copy firmware_image for bank %d to firmware_bank_image - current_firmware_position = 0x%8.8x - remaining_byte_to_write = %d\n" , 
	  firmware_bank_no , current_firmware_position , remaining_bytes_to_write );
  fflush(stdout);

  unsigned int current_page_position = 0;
  while( remaining_bytes_to_write > 0 ) {

       if ( ( current_page_position % 256) == 0 ) {
          printf( "cpci_extract_firmware_bank_from_firmware_image( ) : --> @ bank %d - page %d / addr 0x%4.4x [0x%8.8x] - value = 0x%4.4x (remaining_bytes = %d)\n" , 
		  firmware_bank_no , ( current_page_position/256 ) , current_page_position , current_firmware_position , ice40_firmware_buffer[current_firmware_position] , remaining_bytes_to_write );
	  fflush(stdout);
       }

       firmware_bank_image[current_page_position] = ( ice40_firmware_buffer[current_firmware_position] & 0xff ); /* only want the LS byte */
       firmware_bank_image_size++;

       current_page_position++;
       current_firmware_position++;
       remaining_bytes_to_write--;

  } // end of WHILE-loop over the bank

  if ( trace_debug ) { 
     unsigned int ii = 0;
     for( ii=0 ; ii<firmware_bank_image_size ; ii++ ) {
        printf( "cpci_extract_firmware_bank_from_firmware_image( ) : - firmware_bank_image[%d] = 0x%8.8x <- ice40_firmware_buffer[%d] = 0x%8.8x\n",
		ii                            , firmware_bank_image[ii]                              ,
		firmware_bank_no*bank_size+ii , ice40_firmware_buffer[firmware_bank_no*bank_size+ii] );
	fflush(stdout);
     }
  }

  firmware_bank_image_bank_no = firmware_bank_no;

  if ( firmware_bank_image_size == bank_size ) {
     return 0;
  } else {
     return -5;
  }

}


static PyObject *python_extract_firmware_bank_image_from_firmware_image( PyObject *self , PyObject *args ) {

  unsigned int firmware_bank_no = 8; /* invalid bank number */
  if ( !PyArg_ParseTuple( args , "I" , &(firmware_bank_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( firmware_bank_no )" );
     return NULL;
  }

  if ( ice40_firmware_file_name != NULL ) {
     printf( "python_extract_firmware_bank_image_from_firmware_image( ) : current firmware file - \"%s\"\n",
	     ice40_firmware_file_name );
     fflush(stdout);
  }
  if ( ( ice40_firmware_buffer == NULL ) || ( ice40_firmware_buffer_size < 1 ) ) {
     PyErr_SetString( PyExc_IOError , "ICE40 firmware not stored in memory" );
     return NULL;
  }

  // - extract firmware bank from full firmware image

  int firmware_bank_copy_idval = cpci_extract_firmware_bank_from_firmware_image( firmware_bank_no );
  if ( firmware_bank_copy_idval != 0 ) {
     PyErr_SetString( PyExc_ValueError , "unable to extract firmware bank image from firmware image file" );
     return NULL;
  }

  int return_code = 0;
  if ( firmware_bank_copy_idval < 0 ) {
     return_code = firmware_bank_copy_idval;
  } else {
     return_code = firmware_bank_image_size;
  }

  return Py_BuildValue( "i" , return_code );

}


static PyObject *python_extract_page_from_firmware_bank_image( PyObject *self , PyObject *args ) {

  unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  unsigned int   page_size = 256;     /* number of bits in a single SPI memory page                                        */
//unsigned int   bank_size = 256*256; /* each bank contains 256 pages, each page being 256-bytes in size                   */

  unsigned int page_no = 257; /* invalid page number */
  if ( !PyArg_ParseTuple( args , "I" , &(page_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( page_no )" );
     return NULL;
  }

  if ( ice40_firmware_file_name != NULL ) {
     printf( "python_extract_page_from_firmware_bank_image( ) : current firmware file - \"%s\"\n",
	     ice40_firmware_file_name );
     fflush(stdout);
  }
  if ( ( ice40_firmware_buffer == NULL ) || ( ice40_firmware_buffer_size < 1 ) ) {
     PyErr_SetString( PyExc_IOError , "ICE40 firmware image not stored in memory" );
     return NULL;
  }
  if ( ( firmware_bank_image == NULL ) || ( firmware_bank_image_size < 1 ) ) {
     PyErr_SetString( PyExc_IOError , "ICE40 firmware bank image not stored in memory" );
     return NULL;
  }

  // - extract page from the stored firmware bank image

  unsigned int   remaining_bytes_to_write = page_size;
  unsigned int  current_firmware_position = page_no*page_size;
  
  printf( "python_extract_page_from_firmware_bank_image( ) : -> copy firmware page %d to tuple - current_firmware_position = 0x%8.8x - remaining_byte_to_write = %d\n" , 
	  page_no , current_firmware_position , remaining_bytes_to_write );
  fflush(stdout);

  PyObject *new_tuple = PyTuple_New( page_size );
  if ( new_tuple == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory for tuple to store firmware page" );
     return NULL;
  }

  unsigned int current_tuple_position = 0;
  while( remaining_bytes_to_write > 0 ) {

       if ( trace_debug ) {
	  printf( "python_extract_page_from_firmware_bank_image( ) : --> copy from page %d / position = %d - firmware_bank_image[%d] = 0x%8.8x (remaining_bytes_to_write = %d) \n" , 
		  page_no , current_tuple_position ,
		  current_firmware_position , firmware_bank_image[current_firmware_position] , 
		  remaining_bytes_to_write );
	  fflush(stdout);
       }

       PyTuple_SetItem( new_tuple , current_tuple_position , Py_BuildValue( "I" , firmware_bank_image[current_firmware_position] ) );

       current_tuple_position++;
       current_firmware_position++;
       remaining_bytes_to_write--;

  } // end of WHILE-loop over the bank

  return new_tuple;

}


int cpci_spimemcopy_command( unsigned int command_size , unsigned int command[] ) {

  unsigned int ice40_spimem_register = 0x0E0000;

  unsigned int  regNum_raw = ice40_spimem_register;
  unsigned int      regNum = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  unsigned int ii = 0;
  for( ii=0 ; ii<command_size ; ii++ ) {
     if ( debug_output ) {
        printf( "cpci_spimemcopy_command( ) : - write 0x%8.8x to \"Artix SPI memory\" - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
		(command[ii]&0xff) , regNum , regNum_raw , command[ii] );
     }
     int idval = cpcitest_bar1_write( &dev , regNum , (command[ii]&0xff) );
     if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
	return idval;
     } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
	return idval;
     }
     regNum++;
     regNum_raw += 4;
  }

  return 0;

}


static PyObject *python_cpci_spimem_read( PyObject *self , PyObject *args ) {

  unsigned int ice40_spimem_register = 0x0E0000;                            /* SPI_MEMORY register start position in Artix */

  unsigned int number_of_elements_to_be_read = 0;
  if ( !PyArg_ParseTuple( args , "I" , &(number_of_elements_to_be_read) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( number_of_elements_to_be_read )" );
     return NULL;
  }

  PyObject *new_tuple = PyTuple_New( number_of_elements_to_be_read );
  if ( new_tuple == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory for tuple to store data read back from SPI memory" );
     return NULL;
  }

  unsigned int  regNum_raw = ice40_spimem_register;
  unsigned int      regNum = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  int readout_count = 0;
  for( readout_count = 0 ; readout_count < number_of_elements_to_be_read ; readout_count++ ) {
     if ( ( regNum < 0 ) || ( regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) ) {
        PyErr_SetString( PyExc_ValueError , "register number out of range" );
	return NULL;
     }
     unsigned int idval = cpcitest_bar1_read( &dev, regNum );
     if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
        PyErr_SetString( PyExc_IOError , "return from cpci_bar1_read() : invalid handle - uic_generic_pci device not open" );
        return NULL;
     } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
        PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_read() : register number out of range" );
        return NULL;
     }
     printf( "python_cpci_spimem_read() : - readback sample %4.4d - regNum = 0x%8.8x <- raw_regNum = 0x%8.8x - data = 0x%8.8x\n",
	     readout_count ,
             regNum        ,
             regNum_raw    ,
             idval         );
     PyTuple_SetItem( new_tuple , readout_count , Py_BuildValue( "I" , idval ) );
     regNum++;
     regNum_raw += 4;
  }

  return new_tuple;

}


static PyObject *python_cpci_spimem_write( PyObject *self , PyObject *args ) {

  unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  PyObject    *tuple_of_values_to_write = NULL;
  if ( !PyArg_ParseTuple( args , "O" , &(tuple_of_values_to_write) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( tuple_of_values_to_write_to_memory )" );
     return NULL;
  }

  unsigned int tuple_size = PyTuple_Size( tuple_of_values_to_write );
  if ( debug_output ) {
     printf( "python_cpci_spimem_write( ) : number of elements to write = %d\n" , tuple_size );
     fflush(stdout);
  }

  unsigned int *tuple_values = (unsigned int *) malloc( tuple_size*sizeof(unsigned int) ); // allocate memory to temporarily store values
  if ( tuple_values == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory to temporarily store tuple_values for SPI memory write" );
     return NULL;
  }

  int ii=0;
  for( ii=0 ; ii<tuple_size ; ii++ ) {
     PyObject       *value_pyobject = PyTuple_GetItem( tuple_of_values_to_write , ii );
     unsigned long         value_ul = PyInt_AsUnsignedLongMask( value_pyobject );
     tuple_values[ii] = ( value_ul & 0xff );
     if ( trace_debug ) {
        printf( "python_cpci_spimem_write( ) : input_tuple[%d] = 0x%8.8lx -> tuple_values[%d] = 0x%8.8x" , 
		ii , value_ul ,
		ii , tuple_values[ii] );
	fflush(stdout);
     }
  }

  int return_code = cpci_spimemcopy_command( tuple_size , tuple_values ); 
  if ( return_code != 0 ) {
     if ( tuple_values != NULL ) {
        free( tuple_values );
	tuple_values = NULL;
     }
     return NULL;   /* no need to state reason here as routine does that before exitting */
  }

  if ( tuple_values != NULL ) {
     free( tuple_values );
     tuple_values = NULL;
  }

  return Py_BuildValue( "I" , return_code );

}


int cpci_spiload_command( unsigned int lab_no , unsigned int command_size , unsigned int command[] ) {

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     printf( "cpci_spiload_command( ) : invalid lab number - value = %d\n",lab_no );
     return -1;
  }

  unsigned int    ice40_spiload_register = 0x0C0030;                        /* SPI_LOAD register for ICE40                 */
  unsigned int ice40_spiexecute_register = 0x0C0034;                        /* SPI_EXECUTE register for ICE40              */

  /*          A : load the command into the spi memory region                                                              */

  int spimemcpy_return_code = cpci_spimemcopy_command( command_size , command );
  if ( spimemcpy_return_code != 0 ) {
     return spimemcpy_return_code;    /* no need to state reason here as routine does that before exitting */
  }

  /*          B : transfer the command from artix7 to ice40                                                                */

  unsigned int  lab_addr_field = ( (lab_no&0xf) << 12 );                    /* lab addr                      - data[15:12] */
  unsigned int data_size_field = ( command_size & 0xff );                   /* data_size                     - data[7:0]   */
  unsigned int      data_field = lab_addr_field | data_size_field;

  unsigned int      regNum_raw = ice40_spiload_register;
  unsigned int          regNum = ( regNum_raw >> 2 );    /* divide by 4 to account for bar1_read use of int32 pointer math */

  if ( debug_output ) {
     printf( "cpci_spiload_command( ) : - write 0x%8.8x to \"ICE40 SPI_LOAD\" register to program - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
	     data_field , regNum , regNum_raw , data_field );
  }
  int idval = cpcitest_bar1_write( &dev, regNum , data_field );

#if 1
  /*           C : wait for the ICE40 to complete the operation (Luca bits)                                                */

  regNum_raw = ice40_spiexecute_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  int read_idval = cpcitest_bar1_read( &dev, regNum );
  int  ready_bit = read_idval & 0x1;                    /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  while ( ready_bit != 0 ) {
        read_idval = cpcitest_bar1_read( &dev, regNum );
	ready_bit  = read_idval & 0x1;                  /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  }
#endif

  return idval;

}


static PyObject *python_cpci_spiload_command( PyObject *self , PyObject *args ) {

  unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  unsigned int  lab_no = 0;
  PyObject     *tuple_of_values_to_write = NULL;
  if ( !PyArg_ParseTuple( args , "IO" , &(lab_no) , &(tuple_of_values_to_write) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , tuple_of_values_to_write_to_memory )" );
     return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for communicating with ice40 SPI memory at this time" );
     return NULL;
  }

  unsigned int tuple_size = PyTuple_Size( tuple_of_values_to_write );
  if ( debug_output ) {
     printf( "python_cpci_spiload_command( ) : number of elements to write = %d\n" , tuple_size );
     fflush(stdout);
  }

  unsigned int *tuple_values = (unsigned int *) malloc( tuple_size*sizeof(unsigned int) ); // allocate memory to temporarily store values
  if ( tuple_values == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory to temporarily store tuple_values for SPI memory write" );
     return NULL;
  }

  int ii=0;
  for( ii=0 ; ii<tuple_size ; ii++ ) {
     PyObject       *value_pyobject = PyTuple_GetItem( tuple_of_values_to_write , ii );
     unsigned long         value_ul = PyInt_AsUnsignedLongMask( value_pyobject );
     tuple_values[ii] = ( value_ul & 0xff );
     if ( trace_debug ) {
        printf( "python_cpci_spiload_command( ) : input_tuple[%d] = 0x%8.8lx -> tuple_values[%d] = 0x%8.8x" , 
		ii , value_ul ,
		ii , tuple_values[ii] );
	fflush(stdout);
     }
  }

  int return_code = cpci_spiload_command( lab_no , tuple_size , tuple_values );

  if ( return_code != 0 ) {
     if ( tuple_values != NULL ) {
        free( tuple_values );
	tuple_values = NULL;
     }
     return NULL;   /* no need to state reason here as routine does that before exitting */
  }

  if ( tuple_values != NULL ) {
     free( tuple_values );
     tuple_values = NULL;
  }

  return Py_BuildValue( "I" , return_code );

}


int cpci_spiexecute_command( unsigned int lab_no , unsigned int read_back_data_size ) {

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     printf( "cpci_spiexecute_command( ) : invalid lab number - value = %d\n",lab_no );
     return -1;
  }

//unsigned int     ice40_spimem_register = 0x0E0000;
//unsigned int    ice40_spiload_register = 0x0C0030;                        /* SPI_LOAD register for ICE40                 */
  unsigned int ice40_spiexecute_register = 0x0C0034;                        /* SPI_EXECUTE register for ICE40              */

  printf( "cpci_spiexecute_command( ) : lab_no = %d - read_back_data_size = %d\n",
	  lab_no , read_back_data_size );

  /*          A : execute the command into the spi memory region                                                           */

  unsigned int  lab_addr_field = ( (lab_no&0xf) << 12 );                    /* lab addr                      - data[15:12] */
  unsigned int data_size_field = ( read_back_data_size & 0xff );            /* read_bank_data_size           - data[7:0]   */
  unsigned int      data_field = lab_addr_field | data_size_field;
  unsigned int      regNum_raw = ice40_spiexecute_register;
  unsigned int          regNum = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  if ( debug_output ) {
     printf( "cpci_setexecute_command( ) : - write 0x%8.8x to \"ICE40 SPI_EXECUTE\" register to program - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - data_field = 0x%8.8x\n",
	     data_field , regNum , regNum_raw , data_field );
  }

  int idval = cpcitest_bar1_write( &dev, regNum , data_field );

#if 1
  /*           B : wait for the ICE40 to complete the operation                                                            */

  regNum_raw = ice40_spiexecute_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  int read_idval = cpcitest_bar1_read( &dev, regNum );
  int  ready_bit = read_idval & 0x1;                     /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  while ( ready_bit != 0 ) {
        read_idval = cpcitest_bar1_read( &dev, regNum );
	ready_bit  = read_idval & 0x1;                   /* 0x02 : LAB readout in progress / 0x01 : SPI_EXECUTE in progress */
  }
#endif

  return idval;

}


static PyObject *python_cpci_spiexecute_command( PyObject *self , PyObject *args ) {

  unsigned int  lab_no = 0;
  unsigned int  read_back_data_size = 0;
  if ( !PyArg_ParseTuple( args , "II" , &(lab_no) , &(read_back_data_size) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , read_back_data_size )" );
    return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for executing a SPI memory function at this time" );
     return NULL;
  }

  printf( "python_cpci_spiexecute_command( ) : lab_no = %d - read_back_data_size = %d\n",
	  lab_no , read_back_data_size );

  int return_code = cpci_spiexecute_command( lab_no , read_back_data_size );

  if ( return_code != 0 ) {
     return NULL;   /* no need to state reason here as routine does that before exitting */
  }

  return Py_BuildValue( "I" , return_code );

}


int cpci_spiwriteenable_command( unsigned int lab_no ) {

  int          load_idval = 0;
  int          exec_idval = 0;

  unsigned int ice40_spimem_register     = 0x0E0000;                      /* SPI_MEMORY register start position in Artix */

  unsigned int regNum_raw = 0;
  unsigned int regNum     = 0;

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     printf( "cpci_spiwriteenable_command( ) : invalid lab number - value = %d\n",lab_no );
     return -1;
  }

  /* STEP 1 : write ENABLE the SPI                                                                                       */
  /*          SPI_LOAD, length 1, 0x06 (Write Enable)                                                                    */

  unsigned int spi_write_enable_command_size = 1;
  unsigned int spi_write_enable_command[]    = { 0x06 };

  load_idval = cpci_spiload_command( lab_no , spi_write_enable_command_size , spi_write_enable_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
     return load_idval;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return load_idval;
  }

  /* STEP 2 : execute the write enable command                                                                           */
  /*          SPI_EXECUTE, length 0                                                                                      */

  /*          A : do the SPI_EXECUTE ... no return value                                                                 */

  exec_idval = cpci_spiexecute_command( lab_no , 0 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
     return exec_idval;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return exec_idval;
  }

  /* STEP 3 : read back the status until we see 0x02 ... 0x02 : can write / 0x01 : write in progress)                    */
  /*          SPI_LOAD, length 1, 0x05 (Read Status Register)                                                            */

//unsigned int ready_bit    = 1;
  unsigned int ready_status = 1;
  while ( (ready_status&0x03) != 0x02 ) {

        unsigned int spi_read_enable_command_size = 1;
	unsigned int spi_read_enable_command[]    = { 0x05 };

	load_idval = cpci_spiload_command( lab_no , spi_read_enable_command_size , spi_read_enable_command );
	if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
	   return load_idval;
	} else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
 	   return load_idval;
	}

  /* STEP 4 : execute the read status command                                                                            */
  /*          SPI_EXECUTE, length 1                                                                                      */

  /*          A : do the SPI_EXECUTE                                                                                     */

	exec_idval = cpci_spiexecute_command( lab_no , 1 );
	if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
	   return exec_idval;
	} else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
	   return exec_idval;
	}

  /*          B : fetch the read status value from SPI memory                                                            */

	regNum_raw = ice40_spimem_register;
	regNum     = ( regNum_raw >> 2 );       /* divide by 4 to account for bar1_read use of int32 pointer math */

	ready_status = cpcitest_bar1_read( &dev, regNum );
	if ( ready_status == CPCITEST_ERR_INVALID_HANDLE ) {
	   return ready_status;
	} else if ( ready_status == CPCITEST_ERR_INVALID_REGISTER ) {
	   return ready_status;
	} 
	if ( debug_output ) {
	   printf( "cpci_spiwriteenable_command( ) : - read back 0x%8.8x from \"Artix SPI memory\" register - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - IN LOOP\n",
		   ready_status , regNum , regNum_raw );
	}
  }

  if ( debug_output ) {
     printf( "cpci_spiwriteenable_command( ) : - read back 0x%8.8x from \"Artix SPI memory\" register - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - EXIT LOOP\n",
	     ready_status , regNum , regNum_raw );
  }
  if ( (ready_status&0x02) == 0x02 ) { /* as stated above, expect 0x02 in readback if SPI is write enabled */
     return 0;
  } else {
     return -1;
  }

}


static PyObject *python_cpci_spi_write_enable( PyObject *self , PyObject *args ) {

  unsigned int lab_no = 0;
  if ( !PyArg_ParseTuple( args , "I" , &(lab_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no )" );
     return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for write enabling the ICE40 SPI memory at this time" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }

  printf( "python_cpci_spi_write_enable( ) : lab_no = %d\n", lab_no );
  fflush(stdout);

  int return_code = cpci_spiwriteenable_command( lab_no );

  printf( "python_cpci_spi_write_enable( ) : return_code = %d\n", return_code );
  fflush(stdout);

  if ( return_code != 0 ) {
     return NULL;   /* no need to state reason here as routine does that before exitting */
  }

  return Py_BuildValue( "I" , return_code );

}


int cpci_spi_read_page( unsigned int lab_no , unsigned int spi_bank_no , unsigned int spi_addr_offset , unsigned int page_size , unsigned int *page ) {

  int          load_idval = 0;
  int          exec_idval = 0;
  int       readout_count = 0;

  unsigned int ice40_spimem_register     = 0x0E0000;                      /* SPI_MEMORY register start position in Artix */

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) { 
     printf( "cpci_spi_read_page( ) : invalid lab number - value = %d\n" , lab_no );
     return -1;
  }

  if ( ( spi_bank_no < 0 ) || ( spi_bank_no > 7 ) ) {
     printf( "cpci_spi_read_page( ) : invalid SPI bank number - value = %d\n" , spi_bank_no );
     return -1;
  }

  if ( spi_addr_offset > 0xFFFF ) {
     printf( "cpci_spi_read_page( ) : invalid spi addr offset - value = %d\n" , spi_addr_offset );
     return -1;
  }

  if ( page == NULL ) {
     printf( "cpci_spi_read_page( ) : page storage array needs to malloc() before invoking this subroutine\n" );
     return -1;
  }

  if ( page_size != 256 ) {
     printf( "cpci_spi_read_page( ) : page size restricted to 256 - value = %d\n" , page_size );
     return -1;
  }

  sleep( 1 );

  /* STEP 1 : read first 128-byte half-page from the SPI                                                                 */
  /*          SPI_LOAD, length 1, 0x03 , bank_no , offset_lsb , offset_msb                                               */

  unsigned int spi_read_page_command_size = 4;
  unsigned int spi_read_page_command[]    = { 0x00 , 0x0 , 0x0 , 0x0 };
  spi_read_page_command[0] = 0x03;                                       /* SPI memory read command */
  spi_read_page_command[1] = ( spi_bank_no & 0x7 );                      /* SPI bank number         */
  spi_read_page_command[2] = ( spi_addr_offset & 0xFF00 ) >> 8;          /* SPI addr offset (MSB)   */
  spi_read_page_command[3] = ( spi_addr_offset & 0x00FF );               /* SPI addr offset (LSB)   */
 
  load_idval = cpci_spiload_command( lab_no , spi_read_page_command_size , spi_read_page_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
     return load_idval;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return load_idval;
  }

  /* STEP 2 : execute the read to move the 'page to write' from SPI into ICE40 FIFO and then into Artix ICE40 memory     */
  /*          SPI_EXECUTE, length 128 (expected amount of data to flow back)                                             */

  exec_idval = cpci_spiexecute_command( lab_no , 128 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return exec_idval;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return exec_idval;
  }

  sleep( 1 );

  /* STEP 3 : load read first half-page into page[] array                                                                */

  unsigned int  regNum_raw = ice40_spimem_register;
  unsigned int      regNum = ( regNum_raw >> 2 );      /* divide by 4 to account for bar1_read use of int32 pointer math */
  unsigned int tuple_count = 0;

  for( readout_count = 0 ; readout_count < 128 ; readout_count++ ) {
     if ( ( regNum < 0 ) || ( regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) ) {
        return CPCITEST_ERR_INVALID_REGISTER;
     }
     unsigned int idval = cpcitest_bar1_read( &dev, regNum );
     if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
        return idval;
     } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
        return idval;
     }
     printf( "python_cpci_spimem_read() : - readback sample %4.4d - regNum = 0x%8.8x <- raw_regNum = 0x%8.8x - data = 0x%8.8x\n",
	     readout_count     ,
             regNum            ,
             regNum_raw        ,
             idval             );
     page[tuple_count] = idval;
     tuple_count++;
     regNum++;
     regNum_raw += 4;
  }

  /* STEP 4 : read second 128-byte half-page from the SPI                                                                */
  /*          SPI_LOAD, length 1, 0x03 , bank_no , offset_lsb , offset_msb                                               */

  spi_read_page_command_size = 4;
  spi_read_page_command[0]   = 0x03;                                             /* SPI memory read command */
  spi_read_page_command[1]   = ( spi_bank_no & 0x7 );                            /* SPI bank number         */
  spi_read_page_command[2]   = ( (spi_addr_offset+128) & 0xFF00 ) >> 8;          /* SPI addr offset (MSB)   */
  spi_read_page_command[3]   = ( (spi_addr_offset+128) & 0x00FF );               /* SPI addr offset (LSB)   */
 
  load_idval = cpci_spiload_command( lab_no , spi_read_page_command_size , spi_read_page_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
     return load_idval;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return load_idval;
  }

  /* STEP 5 : execute the read to move the 'page to write' from SPI into ICE40 FIFO and then into Artix ICE40 memory     */
  /*          SPI_EXECUTE, length 128 (expected amount of data to flow back)                                             */

  exec_idval = cpci_spiexecute_command( lab_no , 128 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return exec_idval;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return exec_idval;
  }

  sleep( 1 );

  /* STEP 6 : load read second half-page into page[] array                                                               */

  regNum_raw = ice40_spimem_register;
  regNum     = ( regNum_raw >> 2 );        /* divide by 4 to account for bar1_read use of int32 pointer math */

  for( readout_count = 0 ; readout_count < 128 ; readout_count++ ) {
     if ( ( regNum < 0 ) || ( regNum > (CPCITEST_BAR1_SIZE/sizeof(int)) ) ) {
        return CPCITEST_ERR_INVALID_REGISTER;
     }
     unsigned int idval = cpcitest_bar1_read( &dev, regNum );
     if ( idval == CPCITEST_ERR_INVALID_HANDLE ) {
        return idval;
     } else if ( idval == CPCITEST_ERR_INVALID_REGISTER ) {
        return idval;
     }
     printf( "python_cpci_spimem_read() : - readback sample %4.4d - regNum = 0x%8.8x <- raw_regNum = 0x%8.8x - data = 0x%8.8x\n",
	     readout_count+128 ,
             regNum            ,
             regNum_raw        ,
             idval             );
     page[tuple_count] = idval;    /* will start at page[0+128] since tuple_count carries over from the previous half-page read */
     tuple_count++;
     regNum++;
     regNum_raw += 4;
  }

  return tuple_count;

}


static PyObject *python_cpci_spi_read_page( PyObject *self , PyObject *args ) {

  unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  unsigned int  lab_no = 0;
  unsigned int  spi_bank_no = 0;
  unsigned int  spi_addr_offset = 0;
  if ( !PyArg_ParseTuple( args , "III" , &(lab_no) , &(spi_bank_no) , &(spi_addr_offset) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , spi_bank_no , spi_addr_offset )" );
     return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for communicating with ice40 SPI memory at this time" );
     return NULL;
  }

  if ( ( spi_bank_no < 0 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI bank number - must be between 0 and 7" );
     return NULL;
  }

  unsigned int page_size = 256; /* size of a page */
  if ( debug_output ) {
     printf( "python_cpci_spi_read_page( ) : lab_no = %d / spi_bank_no = %d / spi_addr_offset = 0x%8.8x , elements to read = %d\n" , 
	     lab_no , spi_bank_no , spi_addr_offset , page_size );
     fflush(stdout);
  }

  unsigned int *page = (unsigned int *) malloc( page_size*sizeof(unsigned int) ); // allocate memory to temporarily store values
  if ( page == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory to temporarily store page readback from SPI memory" );
     return NULL;
  }

  int values_read = cpci_spi_read_page( lab_no , spi_bank_no , spi_addr_offset , page_size , page );
  if ( values_read == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( values_read == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  } else if ( values_read < 0 ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_spi_read_page() : general error occurred" );
     return NULL;
  } else if ( values_read != 256 ) {
     printf( "python_cpci_spi_read_page( ) : WARNING - cpci_spi_read_page() returned a partial page, size = %d\n" , values_read );
     fflush(stdout);
  } else {
     printf( "python_cpci_spi_read_page( ) : values_read = %d\n", values_read );
     fflush(stdout);
  }

  PyObject *new_tuple = PyTuple_New( values_read );
  if ( new_tuple == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory for tuple to store page readback from SPI memory" );
     return NULL;
  }

  int ii=0;
  for( ii=0 ; ii<values_read ; ii++ ) {
     PyTuple_SetItem( new_tuple , ii , Py_BuildValue( "I" , page[ii] ) );
     if ( trace_debug ) {
        printf( "python_cpci_spi_read_page( ) : page[%d] = 0x%8.8x -> tuple position %d" ,
		ii , page[ii] , ii );
	fflush(stdout);
     }
  }

  if ( page != NULL ) {
     free( page );
     page = NULL;
  }

  return new_tuple;

}


int cpci_spi_write_page( unsigned int lab_no , unsigned int spi_bank_no , unsigned int spi_addr_offset , unsigned int page_size , unsigned int *page ) {

  int          load_idval = 0;
  int          exec_idval = 0;
  int  write_enable_idval = 0;

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) { 
     printf( "cpci_spi_write_page( ) : invalid lab number - value = %d\n" , lab_no );
     return -1;
  }

  if ( ( spi_bank_no < 2 ) || ( spi_bank_no > 7 ) ) {
     printf( "cpci_spi_write_page( ) : invalid SPI bank number (value = %d) - note: currently restricted to banks 2 to 7" , spi_bank_no );
     return -1;
  }

  if ( spi_addr_offset > 0xFFFF ) {
     printf( "cpci_spi_write_page( ) : invalid spi addr offset - value = %d\n" , spi_addr_offset );
     return -1;
  }

  if ( page_size != 256 ) {
     printf( "cpci_spi_write_page( ) : invalid lab number - value = %d\n" , lab_no );
     return -1;
  }

  if ( page == NULL ) {
     printf( "cpci_spi_write_page( ) : pointer to page storage array is NULL - nothing to write?\n" );
     return -1;
  }

  printf( "cpci_spi_write_page( ) : writing first half-page , page offset addr = 0x%4.4x 0x%4.4x [MSB LSB]\n" ,
          ( (spi_addr_offset) & 0xFF00 ) >> 8 ,   /* SPI addr offset (MSB) */
          ( (spi_addr_offset) & 0x00FF )      );  /* SPI addr offset (LSB) */
  fflush(stdout);

  /* STEP 1 : write ENABLE the SPI                                                                                       */
  /*          SPI_LOAD, length 1, 0x06 (Write Enable)                                                                    */
  /*          SPI_EXECUTE, length 0                                                                                      */
  /* STEP 2 : read back the status (should be 0x02 ... 0x02 : can write / 0x01 : write in progress)                      */
  /*          SPI_LOAD, length 1, 0x05 (Read Status Register)                                                            */
  /*          SPI_EXECUTE, length 1                                                                                      */

  write_enable_idval = cpci_spiwriteenable_command( lab_no );

  if ( write_enable_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return write_enable_idval;
  } else if ( write_enable_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return write_enable_idval;
  }

  sleep( 1 );

  /* STEP 3 : write command for SPI (and specifies the spi address start position)                                       */
  /*          SPI_LOAD, length 1, 0x02 , bank_no , offset_msb , offset_lsb                                               */

  unsigned int spi_write_page_command_size = 4;
  unsigned int spi_write_page_command[]    = { 0x0 , 0x0 , 0x0 , 0x0 };
  spi_write_page_command[0] = 0x02;                                       /* write command         */
  spi_write_page_command[1] = ( spi_bank_no & 0x7 );                      /* SPI bank number       */
  spi_write_page_command[2] = ( spi_addr_offset & 0xFF00 ) >> 8;          /* SPI addr offset (MSB) */
  spi_write_page_command[3] = ( spi_addr_offset & 0x00FF );               /* SPI addr offset (LSB) */
 
//load_idval = cpci_spimemcopy_command( spi_write_page_command_size , spi_write_page_command );
  load_idval = cpci_spiload_command( lab_no , spi_write_page_command_size , spi_write_page_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
     return load_idval;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return load_idval;
  }

  sleep( 1 );

  /* STEP 4 : SPI_LOAD first half page of 'page to write'                                                                */

#if 1
//load_idval = cpci_spimemcopy_command( 128 , page );
  load_idval = cpci_spiload_command( lab_no , 128 , page );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return load_idval; /* no need to state reason here as routine does that before exitting */
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return load_idval;
  }

  sleep( 1 );

  /* STEP 5 : execute the write command to move the 'page to write' from ICE40 FIFO to the SPI                           */
  /*          SPI_EXECUTE, length 0                                                                                      */

#if 1
  exec_idval = cpci_spiexecute_command( lab_no , 0 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return exec_idval;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return exec_idval;
  }
#endif

  sleep( 1 );

  printf( "cpci_spi_write_page( ) : writing second half-page , page offset addr = 0x%4.4x 0x%4.4x [MSB LSB]\n" ,
          ( (spi_addr_offset+128) & 0xFF00 ) >> 8 ,   /* SPI addr offset (MSB) */
          ( (spi_addr_offset+128) & 0x00FF )      );  /* SPI addr offset (LSB) */
  fflush(stdout);

  /* STEP 6 : write ENABLE the SPI                                                                                       */
  /*          SPI_LOAD, length 1, 0x06 (Write Enable)                                                                    */
  /*          SPI_EXECUTE, length 0                                                                                      */
  /* STEP 7 : read back the status (should be 0x02 ... 0x02 : can write / 0x01 : write in progress)                      */
  /*          SPI_LOAD, length 1, 0x05 (Read Status Register)                                                            */
  /*          SPI_EXECUTE, length 1                                                                                      */

  write_enable_idval = cpci_spiwriteenable_command( lab_no );

  if ( write_enable_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return write_enable_idval;
  } else if ( write_enable_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return write_enable_idval;
  }

  sleep( 1 );

  /* STEP 8 : write command for SPI (and specifies the spi address start position)                                       */
  /*          SPI_LOAD, length 1, 0x02 , bank_no , offset_msb , offset_lsb                                               */

  spi_write_page_command_size = 4;
  spi_write_page_command[0] = 0x02;                                       /* write command         */
  spi_write_page_command[1] = ( spi_bank_no & 0x7 );                      /* SPI bank number       */
  spi_write_page_command[2] = ( (spi_addr_offset+128) & 0xFF00 ) >> 8;    /* SPI addr offset (MSB) */
  spi_write_page_command[3] = ( (spi_addr_offset+128) & 0x00FF );         /* SPI addr offset (LSB) */
 
//load_idval = cpci_spimemcopy_command( spi_write_page_command_size , spi_write_page_command );
  load_idval = cpci_spiload_command( lab_no , spi_write_page_command_size , spi_write_page_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) { /* no need to state reason here as routine does that before exitting */
     return load_idval;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return load_idval;
  }

  sleep( 1 );

  /* STEP 9  : SPI_LOAD second half page of 'page to write'                                                              */

//load_idval = cpci_spimemcopy_command( 128 , page+128 );
  load_idval = cpci_spiload_command( lab_no , 128 , page+128 );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return load_idval; /* no need to state reason here as routine does that before exitting */
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return load_idval;
  }
#endif

  sleep( 1 );

  /* STEP 10 : execute the write command to move the 'page to write' from ICE40 FIFO to the SPI                          */
  /*           SPI_EXECUTE, length 0                                                                                     */

#if 1
  exec_idval = cpci_spiexecute_command( lab_no , 0 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return exec_idval;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return exec_idval;
  }
#endif

  sleep( 1 );

  return 0;

}


static PyObject *python_cpci_spi_write_page( PyObject *self , PyObject *args ) {

  unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  unsigned int  lab_no = 0;
  unsigned int  spi_bank_no = 0;
  unsigned int  spi_addr_offset = 0;
  PyObject     *tuple_of_values_to_write = NULL;
  if ( !PyArg_ParseTuple( args , "IIIO" , &(lab_no) , &(spi_bank_no) , &(spi_addr_offset) , &(tuple_of_values_to_write) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , spi_bank_no , spi_addr_offset , tuple_of_values_to_write_to_memory )" );
     return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for communicating with ice40 SPI memory at this time" );
     return NULL;
  }

  if ( ( spi_bank_no < 2 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI bank number - note: currently restricted to banks 2 to 7" );
     return NULL;
  }

  unsigned int tuple_size = PyTuple_Size( tuple_of_values_to_write );
  if ( debug_output ) {
     printf( "python_cpci_spi_write_page( ) : lab_no = %d / spi_bank_no = %d / spi_addr_offset = 0x%8.8x , elements to write = %d\n" , 
	     lab_no , spi_bank_no , spi_addr_offset , tuple_size );
     fflush(stdout);
  }

  unsigned int *tuple_values = (unsigned int *) malloc( tuple_size*sizeof(unsigned int) ); // allocate memory to temporarily store values
  if ( tuple_values == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory to temporarily store tuple_values for SPI memory write" );
     return NULL;
  }

  int ii=0;
  for( ii=0 ; ii<tuple_size ; ii++ ) {
     PyObject       *value_pyobject = PyTuple_GetItem( tuple_of_values_to_write , ii );
     unsigned long         value_ul = PyInt_AsUnsignedLongMask( value_pyobject );
     tuple_values[ii] = ( value_ul & 0xff );
     if ( trace_debug ) {
        printf( "python_cpci_spi_write_page( ) : input_tuple[%d] = 0x%8.8lx -> tuple_values[%d] = 0x%8.8x" , 
		ii , value_ul ,
		ii , tuple_values[ii] );
	fflush(stdout);
     }
  }

  int return_code = cpci_spi_write_page( lab_no , spi_bank_no , spi_addr_offset , tuple_size , tuple_values );

  printf( "python_cpci_spi_write_page( ) : return_code = %d\n", return_code );
  fflush(stdout);

  if ( return_code != 0 ) {
     return NULL;   /* no need to state reason here as routine does that before exitting */
  }

  return Py_BuildValue( "I" , return_code );

}


int cpci_spi_write_page_with_readback_check( unsigned int lab_no , unsigned int spi_bank_no , unsigned int spi_addr_offset , unsigned page_size , unsigned int *page ) {

// unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) { 
     printf( "cpci_spi_write_page_with_readback_check( ) : invalid lab number - value = %d\n" , lab_no );
     return -1;
  }

  if ( ( spi_bank_no < 2 ) || ( spi_bank_no > 7 ) ) {
     printf( "cpci_spi_write_page_with_readback_check( ) : invalid SPI bank number (value = %d) - note: currently restricted to banks 2 to 7" , spi_bank_no );
     return -1;
  }

  if ( spi_addr_offset > 0xFFFF ) {
     printf( "cpci_spi_write_page_with_readback_check( ) : invalid spi addr offset - value = %d\n" , spi_addr_offset );
     return -1;
  }

  if ( page_size != 256 ) {
     printf( "cpci_spi_write_page_with_readback_check( ) : invalid lab number - value = %d\n" , lab_no );
     return -1;
  }

  if ( page == NULL ) {
     printf( "cpci_spi_write_page( ) : pointer to page storage array is NULL - nothing to write?\n" );
     return -1;
  }

  int writepage_idval = cpci_spi_write_page( lab_no , spi_bank_no , spi_addr_offset , page_size , page );
  if ( writepage_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     return writepage_idval; /* no need to state reason here as routine does that before exitting */
  } else if ( writepage_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     return writepage_idval;
  }

  sleep( 1 );
  printf( "cpci_spi_write_page( ) : --> END OF WRITE FOR PAGE (WAIT WAS 1 SECOND)\n" );
  fflush(stdout);

#if 0
  sleep( 4 );
  printf( "cpci_spi_write_page( ) : --> START READBACK (AFTER 4 SECOND WAIT)\n" );
  fflush(stdout);

  unsigned int *readback_page = (unsigned int *) malloc( page_size*sizeof(unsigned int) ); // allocate memory to temporarily store values
  if ( readback_page == NULL ) {
     printf( "unable to allocate memory to temporarily store page readback from SPI memory\n" );
     return -1;
  }

  int values_read = cpci_spi_read_page( lab_no , spi_bank_no , spi_addr_offset , page_size , readback_page );
  if ( values_read == CPCITEST_ERR_INVALID_HANDLE ) {
     return values_read; /* no need to state reason here as routine does that before exitting */
  } else if ( values_read == CPCITEST_ERR_INVALID_REGISTER ) {
     return values_read;
  }

  int return_code = -10;
  if ( values_read != page_size ) {
     printf( "cpci_spi_write_page_with_readback_check : READBACK_FAILED - values_read (%d) != page_size (%d)\n" ,
	     values_read , page_size );
     fflush(stdout);
     return_code = -11;
  }
  else {
     unsigned int error_count = 0;
     unsigned int ii=0;
     for( ii=0 ; ii<values_read ; ii++ ) {
       if ( readback_page[ii] != page[ii] ) {
          error_count++;
	  printf( "cpci_spi_write_page_with_readback_check : page[%d] = 0x%8.8x <-> readback_page[%d] = 0x%8.8x - error_count = %d - ERR [0x%8.8x]\n" ,
		  ii , page[ii] , ii , readback_page[ii] , error_count , page[ii] );
	  fflush(stdout);
       } else {
	 if ( trace_debug ) {
	    printf( "cpci_spi_write_page_with_readback_check : page[%d] = 0x%8.8x <-> readback_page[%d] = 0x%8.8x - error_count = %d\n" ,
		    ii , page[ii] , ii , readback_page[ii] , error_count);
	    fflush(stdout);
	 }
       }
     }
     if ( error_count > 0 ) {
        printf( "cpci_spi_write_page_with_readback_check : READBACK_FAILED - error_count = %d\n" ,
		error_count );
	fflush(stdout);
        return_code = -12;
     } else {
        printf( "cpci_spi_write_page_with_readback_check : READBACK_SUCCESS - error_count = %d\n" ,
		error_count );
	fflush(stdout);
        return_code = 0;
     }
  }

  if ( readback_page != NULL ) {
     free( readback_page );
     readback_page = NULL;
  }
#endif

  int return_code = 0;
  return return_code;

}


static PyObject *python_cpci_spi_write_page_with_readback_check( PyObject *self , PyObject *args ) {

  unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */

  unsigned int  lab_no = 0;
  unsigned int  spi_bank_no = 0;
  unsigned int  spi_addr_offset = 0;
  PyObject     *tuple_of_values_to_write = NULL;
  if ( !PyArg_ParseTuple( args , "IIIO" , &(lab_no) , &(spi_bank_no) , &(spi_addr_offset) , &(tuple_of_values_to_write) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , spi_bank_no , spi_addr_offset , tuple_of_values_to_write_to_memory )" );
     return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for communicating with ice40 SPI memory at this time" );
     return NULL;
  }

  if ( ( spi_bank_no < 2 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI bank number - note: currently restricted to banks 2 to 7" );
     return NULL;
  }

  unsigned int tuple_size = PyTuple_Size( tuple_of_values_to_write );
  if ( debug_output ) {
     printf( "python_cpci_spi_write_page_with_readback_check( ) : lab_no = %d / spi_bank_no = %d / spi_addr_offset = 0x%8.8x , elements to write = %d\n" , 
	     lab_no , spi_bank_no , spi_addr_offset , tuple_size );
     fflush(stdout);
  }

  unsigned int *tuple_values = (unsigned int *) malloc( tuple_size*sizeof(unsigned int) ); // allocate memory to temporarily store values
  if ( tuple_values == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory to temporarily store tuple_values for SPI memory write" );
     return NULL;
  }

  int ii=0;
  for( ii=0 ; ii<tuple_size ; ii++ ) {
     PyObject       *value_pyobject = PyTuple_GetItem( tuple_of_values_to_write , ii );
     unsigned long         value_ul = PyInt_AsUnsignedLongMask( value_pyobject );
     tuple_values[ii] = ( value_ul & 0xff );
     if ( trace_debug ) {
        printf( "python_cpci_spi_write_page_with_readback_check( ) : input_tuple[%d] = 0x%8.8lx -> tuple_values[%d] = 0x%8.8x" , 
		ii , value_ul ,
		ii , tuple_values[ii] );
	fflush(stdout);
     }
  }

  int return_code = cpci_spi_write_page_with_readback_check( lab_no , spi_bank_no , spi_addr_offset , tuple_size , tuple_values );

  printf( "python_cpci_spi_write_page_with_readback_check( ) : return_code = %d\n", return_code );
  fflush(stdout);

  if ( return_code != 0 ) {
     return NULL;   /* no need to state reason here as routine does that before exitting */
  }

  return Py_BuildValue( "I" , return_code );

}


static PyObject *python_cpci_wake_up_ice40( PyObject *self , PyObject *args ) {

  unsigned int          regNum_raw = 0;
  unsigned int              regNum = 0;
  int                   load_idval = 0;
  int                   exec_idval = 0;
  unsigned int          read_idval = 0;

  unsigned int lab_no = 0;
  if ( !PyArg_ParseTuple( args , "I" , &(lab_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no )" );
     return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for programming the ice40 at this time" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }

  unsigned int ice40_spimem_register     = 0x0E0000;                      /* SPI_MEMORY register start position in Artix */

  /* STEP 1 : Release from deep power down/read electronic signature                                                     */
  /*          SPI_LOAD, length 4, 0xAB (Release from Deep Power Down/Read Electronic Signature), 0x00, 0x00, 0x00        */

  unsigned int spi_release_command_size = 4;
  unsigned int spi_release_command[]    = { 0xAB , 0x00 , 0x00 , 0x00 };
  load_idval = cpci_spiload_command( lab_no , spi_release_command_size , spi_release_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  /* STEP 2 : execute the deep power down/read electronic signature command                                              */
  /*          SPI_EXECUTE, length 1 (should return 0x12)                                                                 */

  /*          A : do the SPI_EXECUTE                                                                                     */

  exec_idval = cpci_spiexecute_command( lab_no , 1 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL; 
  }

  sleep( 1 );

  /*          B : read back response from the spi memory region                                                          */

  regNum_raw = ice40_spimem_register;
  regNum     = ( regNum_raw >> 2 );   /* divide by 4 to account for bar1_read use of int32 pointer math */

  read_idval = cpcitest_bar1_read( &dev , regNum );
  if ( read_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( read_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }
  if ( debug_output ) {
     printf( "python_cpci_set_ice40_in_spi_program_mode( ) : - read 0x%8.8x from \"Artix SPI memory\" - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - should be 0x12\n",
	     read_idval , regNum , regNum_raw );
  }

  return Py_BuildValue( "I" , read_idval );

}


static PyObject *python_cpci_set_ice40_in_spi_program_mode( PyObject *self , PyObject *args ) {

  unsigned int          regNum_raw = 0;
  unsigned int              regNum = 0;
  int                   load_idval = 0;
  int                   exec_idval = 0;
  unsigned int          read_idval = 0;
  int           write_enable_idval = 0;

  unsigned int lab_no = 0;
  if ( !PyArg_ParseTuple( args , "I" , &(lab_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no )" );
     return NULL;
  }

  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for programming the ice40 at this time" );
     return NULL;
  }

  if ( !dev.valid ) {
     PyErr_SetString( PyExc_IOError , "invalid handle - uic_generic_pci device not open" );
     return NULL;
  }

  unsigned int ice40_spimem_register     = 0x0E0000;                      /* SPI_MEMORY register start position in Artix */

  /* STEP 1 : Release from deep power down/read electronic signature                                                     */
  /*          SPI_LOAD, length 4, 0xAB (Release from Deep Power Down/Read Electronic Signature), 0x00, 0x00, 0x00        */

  unsigned int spi_release_command_size = 4;
  unsigned int spi_release_command[]    = { 0xAB , 0x00 , 0x00 , 0x00 };
  load_idval = cpci_spiload_command( lab_no , spi_release_command_size , spi_release_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  /* STEP 2 : execute the deep power down/read electronic signature command                                              */
  /*          SPI_EXECUTE, length 1 (should return 0x12)                                                                 */

  /*          A : do the SPI_EXECUTE                                                                                     */

  exec_idval = cpci_spiexecute_command( lab_no , 1 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL; 
  }

  /*          B : read back response from the spi memory region                                                          */

  regNum_raw = ice40_spimem_register;
  regNum     = ( regNum_raw >> 2 );   /* divide by 4 to account for bar1_read use of int32 pointer math */

  read_idval = cpcitest_bar1_read( &dev , regNum );
  if ( read_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( read_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }
  if ( debug_output ) {
     printf( "python_cpci_set_ice40_in_spi_program_mode( ) : - read 0x%8.8x from \"Artix SPI memory\" - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x - should be 0x12\n",
	     read_idval , regNum , regNum_raw );
  }

  /* STEP 3 : sleep 30 microseconds bank command                                                                         */

  usleep( 30 );
  
  /* STEP 4 : write ENABLE the SPI                                                                                       */
  /*          SPI_LOAD, length 1, 0x06 (Write Enable)                                                                    */
  /*          SPI_EXECUTE, length 0                                                                                      */
  /* STEP 5 : read back the status (should be 0x02 ... 0x02 : can write / 0x01 : write in progress)                      */
  /*          SPI_LOAD, length 1, 0x05 (Read Status Register)                                                            */
  /*          SPI_EXECUTE, length 1                                                                                      */

  write_enable_idval = cpci_spiwriteenable_command( lab_no );

  if ( write_enable_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( write_enable_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  /* STEP 6 : return "write enable" status to invoker (should be 0x02 ... 0x02 : can write / 0x01 : write in progress)   */

  int ready_status = cpcitest_bar1_read( &dev, regNum );
  if ( ready_status == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_read() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( ready_status == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_read() : register number out of range" );
     return NULL;
  } 
  if ( debug_output ) {
     printf( "cpci_spiwriteenable_command( ) : - read back 0x%8.8x from \"Artix SPI memory\" register - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x\n",
	     ready_status , regNum , regNum_raw );
  }

  return Py_BuildValue( "I" , ready_status );

}


static PyObject *python_cpci_erase_ice40_spi_bank( PyObject *self , PyObject *args ) {

  unsigned int          regNum_raw = 0;
  unsigned int              regNum = 0;
  int                   load_idval = 0;
  int                   exec_idval = 0;
  int           write_enable_idval = 0;

  unsigned int      lab_no = 0;
  unsigned int spi_bank_no = 0;
  if ( !PyArg_ParseTuple( args , "II" , &(lab_no) , &(spi_bank_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , spi_bank_no )" );
     return NULL;
  }

  if ( spi_bank_no == 0 ) {
     PyErr_SetString( PyExc_ValueError , "restricted - cannot program ICE40 BANK 0" );
     return NULL;
  }
  if ( ( spi_bank_no < 2 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI Bank number - note: currently restricted to banks 2 to 7" );
     return NULL;
  }
  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for programming the ice40 at this time" );
     return NULL;
  }

  unsigned int ice40_spimem_register     = 0x0E0000;                         /* SPI_MEMORY register start position in Artix */

  /* STEP 1 : write ENABLE the SPI                                                                                       */
  /*          SPI_LOAD, length 1, 0x06 (Write Enable)                                                                    */
  /*          SPI_EXECUTE, length 0                                                                                      */
  /* STEP 2 : read back the status (should be 0x02 ... 0x02 : can write / 0x01 : write in progress)                      */
  /*          SPI_LOAD, length 1, 0x05 (Read Status Register)                                                            */
  /*          SPI_EXECUTE, length 1                                                                                      */

  write_enable_idval = cpci_spiwriteenable_command( lab_no );

  if ( write_enable_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( write_enable_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  /* STEP 3 : Setup to erase the SPI bank                                                                                */
  /*          SPI_LOAD, length 4, 0xD8 , spi_bank_no , 0x00 , 0x00                                                       */

  unsigned int spi_erase_bank_command_size = 4;
  unsigned int spi_erase_bank_command[]    = { 0x00 , 0x00 , 0x00 , 0x00 };
  spi_erase_bank_command[0] = 0xD8;                                      /* SPI memory read command */
  spi_erase_bank_command[1] = (spi_bank_no&0x7);                         /* SPI bank number         */
  spi_erase_bank_command[2] = 0x00;                                      /* SPI addr offset (MSB)   */
  spi_erase_bank_command[3] = 0x00;                                      /* SPI addr offset (LSB)   */
  load_idval = cpci_spiload_command( lab_no , spi_erase_bank_command_size , spi_erase_bank_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  /* STEP 4 : execute the erase spi bank command                                                                         */
  /*          SPI_EXECUTE, length 0                                                                                      */

  exec_idval = cpci_spiexecute_command( lab_no , 0 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL; 
  }

  /* STEP 5 : WAIT 5 SECONDS                                                                                             */

  sleep( 5 ); 

  /* STEP 6 : read the WIP bit                                                                                           */
  /*          SPI_LOAD, length 1, 0x05 (Read Status Register)                                                            */

  unsigned int spi_read_enable_command_size = 1;          /* already defined above */
  unsigned int spi_read_enable_command[]    = { 0x05 };

  load_idval = cpci_spiload_command( lab_no , spi_read_enable_command_size , spi_read_enable_command );
  if ( load_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( load_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL;
  }

  /* STEP 7 : do the SPI_EXECUTE                                                                                         */ 
  /*          SPI_EXECUTE, length 1                                                                                      */

  exec_idval = cpci_spiexecute_command( lab_no , 1 );
  if ( exec_idval == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_write() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( exec_idval == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_write() : register number out of range" );
     return NULL; 
  }

  /* STEP 8 : read back the WIP bit - should see bit 0 if things went well                                               */

  regNum_raw = ice40_spimem_register;
  regNum     = ( regNum_raw >> 2 );      /* divide by 4 to account for bar1_read use of int32 pointer math */

  unsigned int erase_status = cpcitest_bar1_read( &dev, regNum );
  if ( erase_status == CPCITEST_ERR_INVALID_HANDLE ) {
     PyErr_SetString( PyExc_IOError , "return from cpci_bar1_read() : invalid handle - uic_generic_pci device not open" );
     return NULL;
  } else if ( erase_status == CPCITEST_ERR_INVALID_REGISTER ) {
     PyErr_SetString( PyExc_ValueError , "return from cpci_bar1_read() : register number out of range" );
     return NULL;
  } 
  if ( debug_output ) {
     printf( "python_cpci_erase_ice40_spi_bank( ) : - read back 0x%8.8x from \"Artix SPI memory\" register - regNum = 0x%8.8x <- regNum_raw = 0x%8.8x\n",
	     erase_status , regNum , regNum_raw );
  }

  return Py_BuildValue( "I" , erase_status );

}


static PyObject *python_cpci_program_ice40_spi( PyObject *self , PyObject *args ) {

  unsigned int trace_debug = 0;     /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */
  unsigned int   page_size = 256;   /* number of bits in a single SPI memory page                                          */

  unsigned int      lab_no = 0;
  unsigned int spi_bank_no = 0;
  if ( !PyArg_ParseTuple( args , "II" , &(lab_no) , &(spi_bank_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( lab_no , spi_bank_no )" );
     return NULL;
  }

  if ( ( spi_bank_no < 0 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI bank number - note: currently restricted to banks 4 to 7" );
     return NULL;
  }
  if ( spi_bank_no == 0 ) {
     PyErr_SetString( PyExc_ValueError , "restricted - cannot program ICE40 BANK 0 ever" );
     return NULL;
  }
  if ( ( spi_bank_no < 4 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI bank number - note: currently restricted to banks 4 to 7" );
     return NULL;
  }
  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for programming the ice40 at this time" );
     return NULL;
  }

  if ( ice40_firmware_file_name != NULL ) {
     printf( "python_cpci_program_ice40_spi( ) : current firmware file - \"%s\"\n",
	     ice40_firmware_file_name );
     fflush(stdout);
  }
  if ( ( ice40_firmware_buffer == NULL ) || ( ice40_firmware_buffer_size < 1 ) ) {
     PyErr_SetString( PyExc_IOError , "ICE40 firmware not stored in memory" );
     return NULL;
  }

  // - write firmware to SPI in chunks of page_size (set to 256 above) byte chunks

  unsigned int         number_of_pages = ice40_firmware_buffer_size / page_size;
  unsigned int       size_of_last_page = ice40_firmware_buffer_size % page_size;
  printf( "python_cpci_program_ice40_spi( ) : need to write %d pages - last page size = %d\n",
	  number_of_pages , size_of_last_page );
  fflush(stdout);
  
  unsigned int  *page = (unsigned int *) malloc( page_size*sizeof(unsigned int) );   // allocate memory to contain one page of the file
                                             
  if ( page == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory for a page of the ICE40 firmware" );
     return NULL;
  }

  unsigned int   remaining_bytes_to_write = ice40_firmware_buffer_size;
  unsigned int  current_firmware_position = 0;
  unsigned int               current_page = 0;
  unsigned int bytes_written_to_spi_flash = 0;                                       // return value for function
  
  while( remaining_bytes_to_write > 0 ) {

       memset( page , 0 , page_size );                                               // -> zero the page memory to avoid any carryover from previous page
       unsigned int current_page_position = 0;
       unsigned int       page_byte_count = 0;
       printf( "python_cpci_program_ice40_spi( ) : --> writing page %d (remaining_bytes_to_write = %d)\n" , 
	       current_page , remaining_bytes_to_write );
       fflush(stdout);
       while ( ( remaining_bytes_to_write > 0 ) && ( page_byte_count < 256 ) ) {     // -> transfer this firmware page to 256-byte page buffer
             if ( trace_debug ) {
	        printf( "python_cpci_program_ice40_spi( ) :    ... page[%3.3d] <- ice40_firmware_buffer[%6.6d] (0x%2.2x) (remaining_bytes_to_write = %d)\n" , 
			current_firmware_position , current_page_position , ice40_firmware_buffer[current_firmware_position] , remaining_bytes_to_write );
		fflush(stdout);
	     }
	     page[current_page_position] = ( ice40_firmware_buffer[current_firmware_position] & 0xff ); /* only want the LS byte */
	     current_page_position++;
	     page_byte_count++;
	     current_firmware_position++;
	     remaining_bytes_to_write--;
       } // end of WHILE-loop
       if ( page_byte_count < 256 ) {                                                // -> if need be, pad the end of the page with zeros
	  while ( page_byte_count < 256 ) {
                printf( "python_cpci_program_ice40_spi( ) :    ... page[%5.5d] <- 0 (remaining_bytes_to_write = %d)\n" , 
			current_page_position , remaining_bytes_to_write );
		fflush(stdout);
		page[current_page_position] = 0;
		current_page_position++;
		page_byte_count++;
	  }
       }

       if ( trace_debug ) { 
	  unsigned int ii = 0;
	  for( ii=0 ; ii<256 ; ii++ ) {
	     printf( "python_cpci_program_ice40_spi( ) : - page %2d / position %3d - byte = 0x%2.2x\n" ,
		     current_page , ii , page[ii] );
	     fflush(stdout);
	  }
       }

#if 1
       int return_code = cpci_spi_write_page_with_readback_check( lab_no , spi_bank_no , current_page*page_size , page_size , page );
       if ( return_code != 0 ) {
	  printf( "python_cpci_program_ice40_spi( ) : - page %2d - page write/readback error - return_code = %d\n" ,
		  current_page , 
		  return_code  );
	  printf( "python_cpci_program_ice40_spi( ) : - page %2d - bytes_written_to_spi_flash = %d - bytes_remaining_to_write = %d\n" ,
		  current_page               ,
		  bytes_written_to_spi_flash ,
		  remaining_bytes_to_write   );
	  fflush(stdout);
	  if ( page != NULL ) {
	     free( page );
	     page = NULL;
	  }
	  return NULL;   /* no need to state reason here as routine does that before exitting */
       }
#endif

       bytes_written_to_spi_flash += page_size;
       current_page++;

  } // end of WHILE-loop over the firmware file

  if ( page != NULL ) {
     free( page );
     page = NULL;
  }

  return Py_BuildValue( "I" , bytes_written_to_spi_flash );

}


static PyObject *python_cpci_program_ice40_spi_bank( PyObject *self , PyObject *args ) {

  unsigned int      trace_debug = 0;       /* == 0 (C false), produce NO trace debug output / != 0 (C true), produce trace output */
  unsigned int        page_size = 256;     /* number of bits in a single SPI memory page                                          */

  unsigned int firmware_bank_no = 0;
  unsigned int           lab_no = 0;
  unsigned int      spi_bank_no = 0;
  if ( !PyArg_ParseTuple( args , "III" , &(firmware_bank_no) , &(lab_no) , &(spi_bank_no) ) ) {
     PyErr_SetString( PyExc_TypeError , "improper arguments, should be ( firmware_bank_no , lab_no , spi_bank_no )" );
     return NULL;
  }

  if ( ( spi_bank_no < 0 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI bank number - note: currently restricted to banks 2 to 7" );
     return NULL;
  }
  if ( spi_bank_no == 0 ) {
     PyErr_SetString( PyExc_ValueError , "restricted - cannot program ICE40 BANK 0 ever" );
     return NULL;
  }
  if ( ( spi_bank_no < 2 ) || ( spi_bank_no > 7 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid SPI bank number - note: currently restricted to banks 2 to 7" );
     return NULL;
  }
  if ( ( lab_no < 0 ) || ( lab_no > 11 ) ) {
     PyErr_SetString( PyExc_ValueError , "invalid LAB number - note: there is no broadcast command for programming the ice40 at this time" );
     return NULL;
  }

  if ( ice40_firmware_file_name != NULL ) {
     printf( "python_cpci_program_ice40_spi_bank( ) : current firmware file - \"%s\"\n",
	     ice40_firmware_file_name );
     fflush(stdout);
  }
  if ( ( ice40_firmware_buffer == NULL ) || ( ice40_firmware_buffer_size < 1 ) ) {
     PyErr_SetString( PyExc_IOError , "ICE40 firmware not stored in memory" );
     return NULL;
  }

  // - extract firmware bank from full firmware image

  int firmware_bank_copy_idval = cpci_extract_firmware_bank_from_firmware_image( firmware_bank_no );
  if ( firmware_bank_copy_idval != 0 ) {
     PyErr_SetString( PyExc_ValueError , "unable to extract image for firmware bank from full firmware image file" );
     return NULL;
  }

  // - write firmware to SPI in chunks of page_size (set to 256 above) byte chunks

  unsigned int         number_of_pages = ice40_firmware_buffer_size / page_size;
  unsigned int       size_of_last_page = ice40_firmware_buffer_size % page_size;
  printf( "python_cpci_program_ice40_spi_bank( ) : need to write %d pages - last page size = %d\n",
	  number_of_pages , size_of_last_page );
  fflush(stdout);

  // - allocate memory for a single 256-byte page

  unsigned int  *page = (unsigned int *) malloc( page_size*sizeof(unsigned int) );   // allocate memory to contain one page of the file
                                             
  if ( page == NULL ) {
     PyErr_SetString( PyExc_MemoryError , "unable to allocate memory for a page of the ICE40 firmware" );
     return NULL;
  }

  // - write each page of the firmware bank image to the specified SPI bank

  unsigned int   remaining_bytes_to_write = firmware_bank_image_size;
  unsigned int  current_firmware_position = 0;
  unsigned int               current_page = 0;
  unsigned int bytes_written_to_spi_flash = 0;                                       // return value for function
  
  while( remaining_bytes_to_write > 0 ) {

       memset( page , 0 , page_size );                                               // -> zero the page memory to avoid any carryover from previous page
       unsigned int current_page_position = 0;
       unsigned int       page_byte_count = 0;
       printf( "python_cpci_program_ice40_spi_bank( ) : --> writing page %d (remaining_bytes_to_write = %d)\n" , 
	       current_page , remaining_bytes_to_write );
       fflush(stdout);
       while ( ( remaining_bytes_to_write > 0 ) && ( page_byte_count < 256 ) ) {     // -> transfer this firmware page to 256-byte page buffer
             if ( trace_debug ) {
	        printf( "python_cpci_program_ice40_spi_bank( ) :    ... page[%3.3d] <-  firmware_bank_image[%6.6d] (0x%2.2x) (remaining_bytes_to_write = %d)\n" , 
			current_firmware_position , current_page_position ,  firmware_bank_image[current_firmware_position] , remaining_bytes_to_write );
		fflush(stdout);
	     }
	     page[current_page_position] = ( firmware_bank_image[current_firmware_position] & 0xff ); /* only want the LS byte */
	     current_page_position++;
	     page_byte_count++;
	     current_firmware_position++;
	     remaining_bytes_to_write--;
       } // end of WHILE-loop
       if ( page_byte_count < 256 ) {                                                // -> if need be, pad the end of the page with zeros
	  while ( page_byte_count < 256 ) {
                printf( "python_cpci_program_ice40_spi_bank( ) :    ... page[%5.5d] <- 0 (remaining_bytes_to_write = %d)\n" , 
			current_page_position , remaining_bytes_to_write );
		fflush(stdout);
		page[current_page_position] = 0;
		current_page_position++;
		page_byte_count++;
	  }
       }

       if ( trace_debug ) { 
	  unsigned int ii = 0;
	  for( ii=0 ; ii<256 ; ii++ ) {
	     printf( "python_cpci_program_ice40_spi_bank( ) : - page %2d / position %3d - byte = 0x%2.2x\n" ,
		     current_page , ii , page[ii] );
	     fflush(stdout);
	  }
       }

       printf( "python_cpci_program_ice40_spi_bank( ) :     ... call cpci_spi_write_page_with_readback_check( %u , %u , 0x%8.8x , %u , page )\n" ,
               lab_no , spi_bank_no , current_page*page_size , page_size );

#if 1
       int return_code = cpci_spi_write_page_with_readback_check( lab_no , spi_bank_no , current_page*page_size , page_size , page );
       if ( return_code != 0 ) {
	  printf( "python_cpci_program_ice40_spi_bank( ) : - page %2d - page write/readback error - return_code = %d\n" ,
		  current_page , 
		  return_code  );
	  printf( "python_cpci_program_ice40_spi_bank( ) : - page %2d - bytes_written_to_spi_flash = %d - bytes_remaining_to_write = %d\n" ,
		  current_page               ,
		  bytes_written_to_spi_flash ,
		  remaining_bytes_to_write   );
	  fflush(stdout);
	  if ( page != NULL ) {
	     free( page );
	     page = NULL;
	  }
	  return NULL;   /* no need to state reason here as routine does that before exitting */
       }
#endif

       bytes_written_to_spi_flash += page_size;
       current_page++;

  } // end of WHILE-loop over the firmware file

  if ( page != NULL ) {
     free( page );
     page = NULL;
  }

  return Py_BuildValue( "I" , bytes_written_to_spi_flash );

}


static char python_cpci_debug_on_docs[]                                   = "cpcitest_open_debug_on_docs( )                                 - turn on debug output from pci reads and writes\n";
static char python_cpci_debug_off_docs[]                                  = "cpcitest_open_debug_off_docs( )                                - turn off debug output from pci reads and writes\n";

static char python_cpci_open_docs[]                                       = "cpcitest_open_docs( )                                          - open up the uio_pci_generic device\n";
static char python_cpci_close_docs[]                                      = "cpcitest_close_docs( )                                         - close the uio_pci_generic device\n";

static char python_cpci_read_bar0_docs[]                                  = "cpcitest_read_bar0_docs( )                                     - read reg N of bar 0 via the uio_pci_generic device\n";
static char python_cpci_read_bar1_docs[]                                  = "cpcitest_read_bar1_docs( )                                     - read reg N of bar 1 via the uio_pci_generic device\n";
static char python_cpci_write_bar0_docs[]                                 = "cpcitest_write_bar0_docs( )                                    - write value to reg N of bar 0 via the uio_pci_generic device\n";
static char python_cpci_write_bar1_docs[]                                 = "cpcitest_write_bar1_docs( )                                    - write value to reg N of bar 1 via the uio_pci_generic device\n";

static char python_cpci_write_glitz_docs[]                                = "cpcitest_cpci_write_glitz_docs( )                              - write value to glitz M at address N via bar 1 of the uio_pci_generic device\n";
static char python_cpci_read_glitz_docs[]                                 = "cpcitest_cpci_read_glitz_docs( )                               - read value from glitz M at address N via bar 1 of the uio_pci_generic device\n";

static char python_cpci_write_long_artix_docs[]                           = "cpcitest_cpci_write_long_artix_docs( )                         - write value to LONG Artix M at address N via bar 1 of the uio_pci_generic device\n";
static char python_cpci_read_long_artix_docs[]                            = "cpcitest_cpci_read_long_artix_docs( )                          - read value from LONG Artix M at address N via bar 1 of the uio_pci_generic device\n";
static char python_cpci_set_long_dac_docs[]                               = "python_cpci_set_long_dac_docs( )                               - set specified DAC on LONG via bar 1 of the uio_pci_generic device\n";
static char python_cpci_set_long_dac_by_channel_docs[]                    = "python_cpci_set_long_dac_by_channel_docs( )                    - set DAC for specified LONG scaler via bar 1 of the uio_pci_generic device\n";

static char python_cpci_read_long_artix_scalers_docs[]                    = "python_cpci_read_long_artix_scalers_docs( )                    - read out the LONG scalers for one Artix via bar 1 of the uio_pci_generic device\n";

static char python_cpci_reboot_ice_docs[]                                 = "cpcitest_cpci_reboot_ice_docs( )                               - reboot ICE N from SPI bank M via bar 1 of the uio_pci_generic device\n";
static char python_cpci_surf_i2c_read_dac_docs[]                          = "cpcitest_cpci_surf_i2c_read_dac_docs( )                        - read external I2C DAC N via bar 1 of the uio_pci_generic device\n";
static char python_cpci_surf_i2c_write_dac_docs[]                         = "cpcitest_cpci_surf_i2c_write_dac_docs( )                       - write data M to external I2C DAC N via bar 1 of the uio_pci_generic device\n";
static char python_cpci_surf_labint_write_dac_docs[]                      = "cpcitest_cpci_surf_labint_write_dac_docs( )                    - read internal LAB DAC N via bar 1 of the uio_pci_generic device\n";
static char python_cpci_write_wraddr_phase_docs[]                         = "python_cpci_write_wraddr_phase_docs( )                         - set \"write address\" phase via bar 1 of the uio_pci_generic device\n";
static char python_cpci_write_lab4c_dTs_docs[]                            = "python_cpci_write_lab4c_dTs_docs( )                            - set LAB4C \"dT\" DAC values via bar 1 of the uio_pci_generic device\n";
static char python_cpci_set_dt_mode_docs[]                                = "python_cpci_set_dt_mode_docs( )                                - set dt mode to common or individual VdlyN via bar 1 of the uio_pci_generic device\n";

static char python_cpci_get_firmware_id_docs[]                            = "cpcitest_cpci_get_firmware_id_docs( )                          - get firmware id for ICE40 N via bar 1 of uio_pci_generic device\n";

static char python_cpci_initialize_surf_for_digitization_docs[]           = "python_cpci_initialize_surf_for_digitization_docs( )           - set hold source to turf (0) or pci (1)\n";
static char python_cpci_hold_lab_bank_docs[]                              = "python_cpci_hold_lab_bank_docs( )                              - hold lab banks specified via 4-bit bank mask\n";
static char python_cpci_digitize_lab_bank_docs[]                          = "python_cpci_digitize_lab_bank_docs( )                          - digitize specified bank (range: 0 to 3)\n";
static char python_cpci_full_digitization_of_lab_docs[]                   = "python_cpci_full_digitization_of_lab_docs( )                   - hold with specified mask, then digitize specified bank (range: 0 to 3)\n";
static char python_cpci_digitize_all_lab_banks_docs[]                     = "python_cpci_digitize_all_lab_banks_docs( )                     - hold and digitize all banks\n";

static char python_cpci_fetch_lab_data_docs[]                             = "python_cpci_fetch_lab_data_docs( )                             - fetch data for specified lab from specified bank (range: 0 to 3)\n";

static char python_cpci_load_ice40_firmware_docs[]                        = "python_cpci_load_ice40_firmware_docs( )                        - read full ICE40 firmware into memory\n";
static char python_extract_firmware_bank_image_from_firmware_image_docs[] = "python_extract_firmware_bank_image_from_firmware_image_docs( ) - extract specified bank from full ICE40 firmware into memory\n";
static char python_extract_page_from_firmware_bank_image_docs[]           = "python_extract_page_from_firmware_bank_image_docs( )           - extract specified bank from stored bank of ICE40 firmware into memory\n";

static char python_cpci_set_ice40_in_spi_program_mode_docs[]              = "python_cpci_set_ice40_in_spi_program_mode_docs( )              - put specified ICE40 into SPI program mode\n";
static char python_cpci_erase_ice40_spi_bank_docs[]                       = "python_cpci_erase_ice40_spi_bank_docs( )                       - erase the program in the specified SPI bank in the specified ICE40\n";
static char python_cpci_program_ice40_spi_docs[]                          = "python_cpci_program_ice40_spi_docs( )                          - program specified bank of specified ICE40 with firmware loaded in memory\n";
static char python_cpci_program_ice40_spi_bank_docs[]                     = "python_cpci_program_ice40_spi_bank_docs( )                     - program specified firmware bank into specified ICE40 with firmware loaded in memory\n";

static char python_cpci_spimem_read_docs[]                                = "python_cpci_spimem_read_docs( )                                - read specified number of elements from Artix SPI memory area into tuple\n";
static char python_cpci_spimem_write_docs[]                               = "python_cpci_spimem_write_docs( )                               - write specified tuple to Artix SPI memory area\n";

static char python_cpci_wake_up_ice40_docs[]                              = "python_cpci_wake_up_ice40_docs( )                              - wake up the specified ICE40\n";
static char python_cpci_spiload_command_docs[]                            = "python_cpci_spiload_command_docs( )                            - write specified tuple of commands to ICE40 SPI memory\n";
static char python_cpci_spiexecute_command_docs[]                         = "python_cpci_spiexecute_command_docs( )                         - execute SPI commands already loaded in the specified ICE40\n";
static char python_cpci_spi_read_page_docs[]                              = "python_cpci_spi_read_page_docs( )                              - read 256-byte page from specified bank of the SPI for the specified ICE40\n";
static char python_cpci_spi_write_enable_docs[]                           = "python_cpci_spi_write_enable_docs( )                           - write enable the SPI for the specified ICE40\n";
static char python_cpci_spi_write_page_docs[]                             = "python_cpci_spi_write_page_docs( )                             - write 256-byte page to specified bank of the SPI for the specified ICE40\n";
static char python_cpci_spi_write_page_with_readback_check_docs[]         = "python_cpci_spi_write_page_with_readback_check_docs( )         - write with a readback check a 256-byte page to specified bank of the SPI for the specified ICE40\n";

static PyMethodDef cpci_funcs[] = {
    { "debugon"                        , (PyCFunction) python_cpci_debug_on                                   , METH_NOARGS  , python_cpci_debug_on_docs                                   } ,
    { "debugoff"                       , (PyCFunction) python_cpci_debug_off                                  , METH_NOARGS  , python_cpci_debug_off_docs                                  } ,
    { "open"                           , (PyCFunction) python_cpci_open                                       , METH_NOARGS  , python_cpci_open_docs                                       } ,
    { "close"                          , (PyCFunction) python_cpci_close                                      , METH_NOARGS  , python_cpci_close_docs                                      } ,
    { "read_bar0"                      , (PyCFunction) python_cpci_read_bar0                                  , METH_VARARGS , python_cpci_read_bar0_docs                                  } ,
    { "read_bar1"                      , (PyCFunction) python_cpci_read_bar1                                  , METH_VARARGS , python_cpci_read_bar1_docs                                  } ,
    { "write_bar0"                     , (PyCFunction) python_cpci_write_bar0                                 , METH_VARARGS , python_cpci_write_bar0_docs                                 } ,
    { "write_bar1"                     , (PyCFunction) python_cpci_write_bar1                                 , METH_VARARGS , python_cpci_write_bar1_docs                                 } ,
    { "write_glitz"                    , (PyCFunction) python_cpci_write_glitz                                , METH_VARARGS , python_cpci_write_glitz_docs                                } ,
    { "read_glitz"                     , (PyCFunction) python_cpci_read_glitz                                 , METH_VARARGS , python_cpci_read_glitz_docs                                 } ,
    { "read_long_artix"                , (PyCFunction) python_cpci_read_long_artix                            , METH_VARARGS , python_cpci_read_long_artix_docs                            } ,
    { "write_long_artix"               , (PyCFunction) python_cpci_write_long_artix                           , METH_VARARGS , python_cpci_write_long_artix_docs                           } ,
    { "set_long_dac"                   , (PyCFunction) python_cpci_set_long_dac                               , METH_VARARGS , python_cpci_set_long_dac_docs                               } ,
    { "set_long_dac_by_channel"        , (PyCFunction) python_cpci_set_long_dac_by_channel                    , METH_VARARGS , python_cpci_set_long_dac_by_channel_docs                    } ,
    { "read_long_artix_scalers"        , (PyCFunction) python_cpci_read_long_artix_scalers                    , METH_VARARGS , python_cpci_read_long_artix_scalers_docs                    } ,
    { "reboot_ice"                     , (PyCFunction) python_cpci_reboot_ice                                 , METH_VARARGS , python_cpci_reboot_ice_docs                                 } ,
    { "surf_i2c_read"                  , (PyCFunction) python_cpci_read_surf_i2c                              , METH_VARARGS , python_cpci_surf_i2c_read_dac_docs                          } ,
    { "surf_i2c_write"                 , (PyCFunction) python_cpci_write_surf_i2c                             , METH_VARARGS , python_cpci_surf_i2c_write_dac_docs                         } ,
    { "surf_lab_write"                 , (PyCFunction) python_cpci_write_labint_dac                           , METH_VARARGS , python_cpci_surf_labint_write_dac_docs                      } ,
    { "surf_set_wraddr_phase"          , (PyCFunction) python_cpci_write_wraddr_phase                         , METH_VARARGS , python_cpci_write_wraddr_phase_docs                         } ,
    { "surf_lab4c_write_dts"           , (PyCFunction) python_cpci_write_lab4c_dTs                            , METH_VARARGS , python_cpci_write_lab4c_dTs_docs                            } ,
    { "surf_lab4c_set_dt_mode"         , (PyCFunction) python_cpci_set_dt_mode                                , METH_VARARGS , python_cpci_set_dt_mode_docs                                } ,
    { "surf_get_firmware_id"           , (PyCFunction) python_cpci_get_firmware_id                            , METH_VARARGS , python_cpci_get_firmware_id_docs                            } ,
    { "surf_hold_source"               , (PyCFunction) python_cpci_initialize_surf_for_digitization           , METH_VARARGS , python_cpci_initialize_surf_for_digitization_docs           } ,
    { "surf_hold_lab_bank"             , (PyCFunction) python_cpci_hold_lab_bank                              , METH_VARARGS , python_cpci_hold_lab_bank_docs                              } ,
    { "surf_digitize_lab_bank"         , (PyCFunction) python_cpci_digitize_lab_bank                          , METH_VARARGS , python_cpci_digitize_lab_bank_docs                          } ,
    { "surf_full_digitize_lab_bank"    , (PyCFunction) python_cpci_full_digitization_of_lab                   , METH_VARARGS , python_cpci_full_digitization_of_lab_docs                   } ,
    { "surf_full_digitize_all_banks"   , (PyCFunction) python_cpci_digitize_all_lab_banks                     , METH_NOARGS  , python_cpci_digitize_all_lab_banks_docs                     } ,
    { "surf_read_data"                 , (PyCFunction) python_cpci_fetch_lab_data                             , METH_VARARGS , python_cpci_fetch_lab_data_docs                             } ,
    { "spi_artix_memcpy"               , (PyCFunction) python_cpci_spimem_write                               , METH_VARARGS , python_cpci_spimem_write_docs                               } ,
    { "spi_artix_memread"              , (PyCFunction) python_cpci_spimem_read                                , METH_VARARGS , python_cpci_spimem_read_docs                                } ,
    { "ice40_wake_up"                  , (PyCFunction) python_cpci_wake_up_ice40                              , METH_VARARGS , python_cpci_wake_up_ice40_docs                              } ,
    { "spi_load"                       , (PyCFunction) python_cpci_spiload_command                            , METH_VARARGS , python_cpci_spiload_command_docs                            } ,
    { "spi_execute"                    , (PyCFunction) python_cpci_spiexecute_command                         , METH_VARARGS , python_cpci_spiexecute_command_docs                         } ,
    { "spi_read_page"                  , (PyCFunction) python_cpci_spi_read_page                              , METH_VARARGS , python_cpci_spi_read_page_docs                              } ,
    { "spi_write_enable"               , (PyCFunction) python_cpci_spi_write_enable                           , METH_VARARGS , python_cpci_spi_write_enable_docs                           } ,
    { "spi_write_page"                 , (PyCFunction) python_cpci_spi_write_page                             , METH_VARARGS , python_cpci_spi_write_page_docs                             } ,
    { "spi_write_page_with_check"      , (PyCFunction) python_cpci_spi_write_page_with_readback_check         , METH_VARARGS , python_cpci_spi_write_page_with_readback_check_docs         } ,
    { "load_ice40_firmware"            , (PyCFunction) python_cpci_load_ice40_firmware                        , METH_VARARGS , python_cpci_load_ice40_firmware_docs                        } ,
    { "load_ice40_firmware_bank"       , (PyCFunction) python_extract_firmware_bank_image_from_firmware_image , METH_VARARGS , python_extract_firmware_bank_image_from_firmware_image_docs } ,
    { "check_ice40_firmware_page"      , (PyCFunction) python_extract_page_from_firmware_bank_image           , METH_VARARGS , python_extract_page_from_firmware_bank_image_docs           } ,
    { "setup_for_spi_programming"      , (PyCFunction) python_cpci_set_ice40_in_spi_program_mode              , METH_VARARGS , python_cpci_set_ice40_in_spi_program_mode_docs              } ,
    { "erase_ice40"                    , (PyCFunction) python_cpci_erase_ice40_spi_bank                       , METH_VARARGS , python_cpci_erase_ice40_spi_bank_docs                       } ,
    { "program_ice40"                  , (PyCFunction) python_cpci_program_ice40_spi                          , METH_VARARGS , python_cpci_program_ice40_spi_docs                          } ,
    { "program_ice40_bank"             , (PyCFunction) python_cpci_program_ice40_spi_bank                     , METH_VARARGS , python_cpci_program_ice40_spi_bank_docs                     } ,


    { NULL , NULL , 0 , NULL }                                                                              /* Sentinal to mark end of the python function list */
};

void initcpci( void )
{

    Py_InitModule3( "cpci" , cpci_funcs , "cpci memory mapped interface" );
    Py_AtExit( python_cpci_atexit );
//  printf( "return from Py_InitModule3() happened\n" );
    return;

}

