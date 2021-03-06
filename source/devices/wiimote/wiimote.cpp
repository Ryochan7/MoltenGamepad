#include "wiimote.h"
#include <iostream>
#include <cstring>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
wiimote::wiimote(slot_manager* slot_man) : wii_dev(slot_man) {
    for (int i = 0; i < wii_event_max; i++) {
      register_event(wiimote_events[i]);
    }
    for (int i = 0; !wiimote_options[i].name.empty(); i++) {
      register_option(wiimote_options[i]);
    }
    
}

wiimote::~wiimote() {
   clear_node(&buttons);
   clear_node(&accel);
   clear_node(&ir);
   clear_node(&motionplus);
   clear_node(&nunchuk);
   clear_node(&classic);
   
   
   
   
   free (nameptr);
}

int wiimote::process_option(const char* name, const char* value) {
  if (!strcmp(name,"wm_accel_active")) {
    if (!strcmp(value,"true")) { wm_accel_active = true; update_mode(); return 0;};
    if (!strcmp(value,"false")) { wm_accel_active = false; update_mode(); return 0;};
    return -1;
  }
  if (!strcmp(name,"nk_accel_active")) {
    if (!strcmp(value,"true")) { nk_accel_active = true; update_mode(); return 0;};
    if (!strcmp(value,"false")) { nk_accel_active = false; update_mode(); return 0;};
    return -1;
  }
  if (!strcmp(name,"wm_ir_active")) {
    if (!strcmp(value,"true")) { wm_ir_active = true; update_mode(); return 0;};
    if (!strcmp(value,"false")) { wm_ir_active = false; update_mode(); return 0;};
    return -1;
  }
  if (!strcmp(name,"nk_ir_active")) {
    if (!strcmp(value,"true")) { nk_ir_active = true; update_mode(); return 0;};
    if (!strcmp(value,"false")) { nk_ir_active = false; update_mode(); return 0;};
    return -1;
  }
  
  return -1;
}

void wiimote::clear_node(struct dev_node* node) {
  if (node->dev) udev_device_unref(node->dev);
  node->dev = nullptr;
  if (node->fd >= 0) close(node->fd);
}

/*NOTE: Finding uses prefixes, but destroying needs an exact match.
  This allows easily adding/removing subnodes while only deleting
  if the base node is removed                                    */
wii_dev* find_wii_dev_by_path(std::vector<wii_dev*>* devs, const char* syspath) {
  if (syspath == nullptr) return nullptr;
  for (auto it = devs->begin(); it != devs->end(); ++it) {
    const char* devpath = udev_device_get_syspath( (*it)->base.dev);
    if (!devpath) continue;
    
    if (strstr(syspath,devpath) == syspath) return (*it);
  }
  return nullptr;
}

int destroy_wii_dev_by_path(std::vector<wii_dev*>* devs, const char* syspath) {
  if (syspath == nullptr) return -1;
  for (auto it = devs->begin(); it != devs->end(); ++it) {
    const char* devpath = udev_device_get_syspath( (*it)->base.dev);
    if (!devpath) continue;
    
    if (!strcmp(devpath,syspath)) {
      device_delete_lock.lock();
      delete *it;
      devs->erase(it);
      device_delete_lock.unlock();
      return 0;
    }
  }
  return -1;
}



void add_led(struct wii_leds* leds, struct udev_device* dev) {
  //TODO: Store LED files in the wiimote.
}

void wiimote::handle_event(struct udev_device* dev) {
  const char* subsystem = udev_device_get_subsystem(dev);
  const char* action = udev_device_get_action(dev);
  
  if (action && !strcmp(action,"remove")) {
    const char* nodename = udev_device_get_sysattr_value(udev_device_get_parent(dev),"name");
    if (!nodename) nodename = "";
    remove_node(nodename);
    return;
  }
  if (!subsystem || !strcmp(subsystem,"hid")) return; //Nothing to do.

  if (!strcmp(subsystem,"led")) {
    add_led(&leds, dev);
  }

  if (!strcmp(subsystem,"input")) {
    const char* sysname = udev_device_get_sysname(dev);
    const char* name = nullptr; 


    if (!strncmp(sysname,"event",3)) {
      struct udev_device* parent = udev_device_get_parent(dev);

      name = udev_device_get_sysattr_value(parent,"name");

      store_node(dev,name);

    }
    
    update_mode();

  
  }
}

enum nodes {CORE, E_NK, E_CC,  IR, ACCEL, MP, NONE};
int name_to_node(const char* name) {
  if (!strcmp(name, WIIMOTE_NAME)) return CORE;
  if (!strcmp(name, WIIMOTE_IR_NAME)) return IR;
  if (!strcmp(name, WIIMOTE_ACCEL_NAME)) return ACCEL;
  if (!strcmp(name, MOTIONPLUS_NAME)) return MP;
  if (!strcmp(name, NUNCHUK_NAME)) return E_NK;
  if (!strcmp(name, CLASSIC_NAME)) return E_CC;
  return NONE;
}

void wiimote::store_node(struct udev_device* dev, const char* name) {
  if (!name || !dev) return;
  int node = name_to_node(name);
  switch (node) {
  case CORE:
    std::cout<< this->name << " core found." << std::endl;
    
    buttons.dev = udev_device_ref(dev);
    open_node(&buttons);
    break;
  case IR:
    std::cout<< this->name << " IR found." << std::endl;
    ir.dev = udev_device_ref(dev);
    if ((mode == NO_EXT && wm_ir_active) || (mode == NUNCHUK_EXT && nk_ir_active))
      open_node(&ir);
    break;
  case ACCEL:
    std::cout<< this->name << " accelerometers found." << std::endl;
    accel.dev = udev_device_ref(dev);
    if ((mode == NO_EXT && wm_accel_active) || (mode == NUNCHUK_EXT && nk_accel_active))
      open_node(&ir);
    break;
  case MP:
    std::cout<< this->name << " motion+ found." << std::endl;
    motionplus.dev = udev_device_ref(dev);
    break;
  case E_NK:
    mode = NUNCHUK_EXT;
    nunchuk.dev = udev_device_ref(dev);
    open_node(&nunchuk);
    update_mode();
    std::cout<< this->name << " gained a nunchuk." << std::endl;
    break;
  case E_CC:
    mode = CLASSIC_EXT;
    classic.dev = udev_device_ref(dev);
    open_node(&classic);
    update_mode();
    std::cout<< this->name << " gained a classic controller." << std::endl;
    break;
  }


}

void wiimote::update_mode() {
  if ((mode == NO_EXT && wm_ir_active) || (mode == NUNCHUK_EXT && nk_ir_active)) {
    if (ir.dev != nullptr && ir.fd < 0)
      open_node(&ir);
  } else {
    if (ir.fd > 0) {
      close(ir.fd);
      ir.fd = -1;
    }
  }
  if ((mode == NO_EXT && wm_accel_active) || (mode == NUNCHUK_EXT && nk_accel_active)) {
    if (accel.dev != nullptr && accel.fd < 0)
      open_node(&accel);
  } else {
    if (accel.fd > 0) {
      close(accel.fd);
      accel.fd = -1;
    }
  }
}

void wiimote::remove_node(const char* name) {
  if (!name) return;
  int node = name_to_node(name);
  if (node == E_NK) {clear_node(&nunchuk); remove_extension();};
  if (node == E_CC) {clear_node(&classic); remove_extension();};
  if (node == MP) {
    std::cout << this->name << " motion+ removed.";
    clear_node(&motionplus);
  }
}

void wiimote::list_events(cat_list &list) {
  struct category cat;
  struct name_descr info;
  
  cat.name = "Wiimote";
  for (int i = 0; i < events.size(); i++) {
    info.name = wiimote_events[i].name;
    info.descr = wiimote_events[i].descr;
    info.data = wiimote_events[i].type;
    cat.entries.push_back(info);
  }
  
  list.push_back(cat);
  cat.entries.clear();
}

void wiimote::open_node(struct dev_node* node) {
  node->fd = open(udev_device_get_devnode(node->dev), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (node->fd < 0)
    perror("open subdevice:");
  ioctl(node->fd, EVIOCGRAB, this);
  
  watch_file(node->fd,node);
};



int lookup_wii_event(const char* evname) {
  if (evname == nullptr) return -1;
  for (int i = 0; i < wii_event_max; i++) {
    if (!strcmp(evname,wiimote_events[i].name))
      return i;
  }
  return -1;
}



enum entry_type wiimote::entry_type(const char* name) {
  int ret = lookup_wii_event(name);
  if (ret != -1) {
    return events[ret].type;
  }
  
  return NO_ENTRY;
}

enum entry_type wiimotes::entry_type(const char* name) {
  int ret = lookup_wii_event(name);
  if (ret != -1) {
    return wiimote_events[ret].type;
  }
  
  return NO_ENTRY;
}



void wiimote::process(void* tag) {
  int type = CORE;
  if (tag == &classic) type = E_CC;
  if (tag == &nunchuk) type = E_NK;
  if (tag == &ir) type = IR;
  if (tag == &accel) type = ACCEL;
  switch(type) {
    case CORE:
      process_core();
      break;
    case E_CC:
      process_classic(classic.fd);
      break;
    case E_NK:
      process_nunchuk(nunchuk.fd);
      break;
    case IR:
      process_ir(ir.fd);
      break;
    case ACCEL:
      process_accel(accel.fd);
      break;
  }
}

int wiimotes::accept_device(struct udev* udev, struct udev_device* dev) {
  const char* path = udev_device_get_syspath(dev);
  const char* subsystem = udev_device_get_subsystem(dev);

  const char* action = udev_device_get_action(dev);
  if (!action) action = "null";

  if (!strcmp(action,"remove")) {

    const char* syspath = udev_device_get_syspath(dev);
    wii_dev* existing = find_wii_dev_by_path(&wii_devs, syspath);

    if (existing) {
      int ret = destroy_wii_dev_by_path(&wii_devs, syspath);
      if (ret) {
        //exact path wasn't found, therefore this is not a parent device.
        existing->handle_event(dev);
      } else {
        //exact path found, and destroyed.
        std::cout << "Wii device removed." << std::endl;
      }
      return 0;
    }
    return -1;
    

  }
  if (!subsystem) return -1;

  struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype(dev,"hid",nullptr);
  if (!strcmp(udev_device_get_subsystem(dev),"hid")) {
    parent = dev; //We are already looking at it!
  }
  if (!parent) return -2;

  const char* driver = udev_device_get_driver(parent);
  if (!driver || strcmp(driver,"wiimote")) return -2;

  const char* parentpath = udev_device_get_syspath(parent);

  wii_dev* existing = find_wii_dev_by_path(&wii_devs, parentpath);

  if (existing == nullptr) {
    //time to add a device;
    std::cout << "Wiimote found (count:" << wii_devs.size() << " )" << std::endl;;
    wiimote* wm = new wiimote(slot_man);
    char* devname;
    asprintf(&devname, "wm%d",++dev_counter);
    wm->name = devname;
    wm->nameptr = devname;
    wm->base.dev = udev_device_ref(parent);
    wm->handle_event(dev);
    wii_devs.push_back(wm);
    wm->start_thread();
    wm->load_profile(&mapprofile);
  } else {
    //pass this device to it for proper storage
    existing->handle_event(dev);
  }


  return 0;
}

void wiimotes::update_maps(const char* evname, event_translator* trans) {
  mapprofile.set_mapping(evname, trans->clone());
  for (auto it = wii_devs.begin(); it != wii_devs.end(); it++)
    (*it)->update_map(evname,trans);
}
void wiimotes::update_chords(const char* ev1, const char* ev2, event_translator* trans) {
  mapprofile.set_chord(ev1, ev2, trans->clone());
  for (auto it = wii_devs.begin(); it != wii_devs.end(); it++)
    (*it)->update_chord(ev1,ev2,trans);
}
void wiimotes::update_options(const char* opname, const char* value) {
  mapprofile.set_option(opname, value);
  for (auto it = wii_devs.begin(); it != wii_devs.end(); it++)
    (*it)->update_option(opname,value);
}

input_source* wiimotes::find_device(const char* name) {
  for (auto it = wii_devs.begin(); it != wii_devs.end(); it++) {
    if (!strcmp((*it)->name,name)) return (*it);
  }
  return nullptr;
}

