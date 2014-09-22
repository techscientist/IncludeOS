#include <hw/class_pci_device.hpp>
#include <hw/class_nic.hpp>
#include <assert.h>

//TODO: Virtio stuff might be separate
#include <virtio/virtio.h>

//enum{CLASS_NIC=,CLA

//Private constructor; only called if "Create" finds a device on this addr.

enum{CL_OLD,CL_STORAGE,CL_NIC,CL_DISPLAY,CL_MULTIMEDIA,CL_MEMORY,CL_BRIDGE};

static const char* classcodes[]={
  "Too-Old-To-Tell",
  "Mass Storage Controller",
  "Network Controller",
  "Display Controller",
  "Multimedia Controller",
  "Memory Controller",
  "Bridge",
  NULL
};

const int SS_BR=3;
static const char* bridge_subclasses[SS_BR]={
  "Host",
  "ISA",
  "Other"
};

const int SS_NIC=2;
static const char* nic_subclasses[SS_NIC]={
  "Ethernet",
  "Other"
};


enum{VENDOR_INTEL=0x8086,VENDOR_CIRRUS=0x1013,VENDOR_REALTEK=0x10EC,
     VENDOR_VIRTIO=0x1AF4,VENDOR_AMD=0x1022};

struct _pci_vendor{
  uint16_t id;
  const char* name;
} _pci_vendorlist[]={
  {0x8086,"Intel Corp."},
  {0x1013,"Cirrus Logic"},
  {0x10EC,"Realtek Semi.Corp."},
  {0x1AF4,"Virtio (Rusty Russell)"}, //Virtio creator
  {0x1022,"AMD"},
  {0x0000,NULL}
};


static unsigned long pci_size(unsigned long base, unsigned long mask)
{
  // Find the significant bits
  unsigned long size = mask & base;

  // Get the lowest of them to find the decode size
  size = size & ~(size-1);

  return size;
}


uint32_t PCI_Device::iobase(){
  if(!res_io_)
    panic("Didn't get any IO-resource from PCI device");
  return res_io_->start_;  
};

void PCI_Device::probe_resources(){

  //Find resources on this PCI device (scan the BAR's)
  uint32_t value=PCI_WTF;
  
  uint32_t reg{0},len{0};
  for(int bar=0; bar<6; bar++){

    //Read the current BAR register   
    reg = PCI_CONFIG_BASE_ADDR_0 + (bar << 2);
    value = read_dword(reg);

    if (!value) continue;

    //Write all 1's to the register, to get the "Real" value (osdev)
    write_dword(reg,0xFFFFFFFF);
    len = read_dword(reg);
    
    uint32_t unmasked_val=0, pci_size_=0;

    if (value & 1) {  // Resource type IO

      unmasked_val = value & PCI_BASE_ADDRESS_IO_MASK;
      pci_size_ = pci_size(len,PCI_BASE_ADDRESS_IO_MASK & 0xFFFF );
      
      //Add it to resource list
      add_resource<RES_IO>(new Resource<RES_IO>(unmasked_val,pci_size_),res_io_);
      assert(res_io_ != 0);

      printf("Added IO resource, start addres: '0x%lx' \n",res_io_->start_);
      
    } else { //Resource type Mem

      unmasked_val = value & PCI_BASE_ADDRESS_MEM_MASK;
      pci_size_ = pci_size(len,PCI_BASE_ADDRESS_MEM_MASK);

      //Add it to resource list
      add_resource<RES_MEM>(new Resource<RES_MEM>(unmasked_val,pci_size_),res_mem_);
      assert(res_io_ != 0);
}    

          
    //DEBUG: Print
    printf("\n\t BAR %i \n"\
           "\t Value: 0x%lx Unmasked:  0x%lx \n "\
           "\t Size: 0x%lx pci_size: 0x%lx \n"\
           "\t Type: %s\n",
           bar,
           value,
           unmasked_val,
           len,           
           pci_size_,
           value & 1 ? "IO Resource" : "Memory Resource");

}
  

  //We should now be able to get the right iobase
  printf("\n\t IO-base: 0x%lx \n",iobase());

  //TRY virtio stuff
  
  //Reset device
  /*
  outp(iobase() + VIRTIO_PCI_STATUS, 0);
  outp(iobase() + VIRTIO_PCI_STATUS, inp(iobase() + VIRTIO_PCI_STATUS) | VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);
  
  uint32_t features=inpd(iobase()+VIRTIO_PCI_HOST_FEATURES);
  printf("\t VIRTIO Features: 0x%lx \n",features);
  */
  
  printf("VIRTIO Mac 1: 0x%lx \n",inpd(iobase() + 0x14));

  uint32_t irq=0,intrpin=0;

  //Get device IRQ 
  value = read_dword(PCI_CONFIG_INTR);
  if ((value & 0xFF) > 0 && (value & 0xFF) < 32){
    intrpin = (value >> 8) & 0xFF;
    irq = value & 0xFF;
  }
  
  if(irq)
    printf("\n\t IRQ: %li, intrpin: %li \n",irq,intrpin);
  
  
  
}

PCI_Device::PCI_Device(uint16_t _pci_addr,uint32_t _id)
  : pci_addr(_pci_addr), device_id{_id}
//,Device(Device::PCI) //Why not inherit Device? Well, I think "PCI devices" are too general to be useful by itself, and the "Device" class is Public ABI, so it should only know about stuff that's relevant for the user.
{
  //We have device, so probe for details
  devtype.reg=read_dword(_pci_addr,PCI_CONFIG_CLASS_REV);
  //printf("\t * New PCI Device: Vendor: 0x%x Prod: 0x%x Class: 0x%x\n", 
  //device_id.vendor,device_id.product,classcode);
  
  printf("\t |\n");  
  
  switch (devtype.classcode) {
    
  case CL_BRIDGE:
    printf("\t +--+ %s %s (0x%x)\n",
           bridge_subclasses[devtype.subclass < SS_BR ? devtype.subclass : SS_BR-1],
           classcodes[devtype.classcode],devtype.subclass);
    break;
  case CL_NIC:
    printf("\t +--+ %s %s (0x%x)\n",
           nic_subclasses[devtype.subclass < SS_NIC ? devtype.subclass : SS_NIC-1],
           classcodes[devtype.classcode],devtype.subclass); 
    printf("\t |  |    \n" );    
    
    if (device_id.vendor!=VENDOR_VIRTIO)
      panic("Only virtio supported");
    
    printf("\t |  +-o (Vendor: Virtio, Product: 0x%x)\n",
           device_id.product);
    
    Dev::add(new Nic(this));
    
    break;
  default:
    printf("\t +--+ %s \n",classcodes[devtype.classcode]);
  }

  
}






uint16_t PCI_Device::get_pci_addr(){return pci_addr;};

  
  //TODO: Subclass this (or add it as member to a class) into device types.
  //...At least for NICs and HDDs
  
