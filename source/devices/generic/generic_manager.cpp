#include "generic.h"

generic_manager::generic_manager(slot_manager* slot_man, generic_driver_info &descr) : device_manager(slot_man) {
  this->name = descr.name.c_str();
  this->devname = descr.devname.c_str();
  mapprofile.name = name;
  
  this->descr = &descr;
  
  split = descr.split;
  flatten = descr.flatten;
  
  if (split > 1) {
    for (int i = 1; i <= split; i++) {
      splitevents.push_back(std::vector<gen_source_event>());
    }
    
    for (auto gen_ev : descr.events) {
      if (gen_ev.split_id < 1 || gen_ev.split_id > split) continue;
      splitevents.at(gen_ev.split_id-1).push_back(gen_ev);
      
    }
    
  }
  
  
  std::cout << name <<" driver initialized." << std::endl;
  
}

generic_manager::~generic_manager() {
  
  for (auto file : openfiles) {
    delete file;
  }
  
  delete descr;
}

int generic_manager::accept_device(struct udev* udev, struct udev_device* dev) {
  const char* path = udev_device_get_syspath(dev);
  const char* subsystem = udev_device_get_subsystem(dev);
  const char* action = udev_device_get_action(dev);
  if (!action) action = "null";

  if (!strcmp(action,"remove")) {
    if (!path) return -1;
    for (auto it = openfiles.begin(); it != openfiles.end(); it++) {
      auto nodes = &((*it)->nodes);
      for (auto nodeit = nodes->begin(); nodeit != nodes->end(); nodeit++) {
        const char* searchpath = udev_device_get_syspath(*nodeit);
        if (!searchpath) { std::cout << ":(" << std::endl; return -1;};
        if (!strcmp(path,searchpath)) {
          std::cout << "GEN DEVICE DESTROYED." << std::endl;
          udev_device_unref(*nodeit);
          (*it)->nodes.erase(nodeit);
          if ((*it)->nodes.empty()) {
            delete (*it);
            openfiles.erase(it);
          }
          return 0;
        }
      }
    }
  }
  if (!strcmp(action,"add") || !strcmp(action,"null")) {
    if (!strcmp(subsystem,"input")) {
      const char* sysname = udev_device_get_sysname(dev);
      const char* name = nullptr; 
      if (!strncmp(sysname,"event",3)) {
        struct udev_device* parent = udev_device_get_parent(dev);
        name = udev_device_get_sysattr_value(parent,"name");
        if (!name) return -2;
         
         
        for (auto it = descr->matches.begin(); it != descr->matches.end(); it++) {
          if (!strcmp((*it).name.c_str(),name)) {
            open_device(udev,dev);
            
          }
        }
      }
    }
  }
  
  
  
  return -2;
}

int generic_manager::open_device(struct udev* udev, struct udev_device* dev) {
  if (flatten) {
    if (openfiles.size() < 1) {
      openfiles.push_back(new generic_file(dev,descr->grab_exclusive));
      create_inputs(openfiles.front(),openfiles.front()->fds.front(),false);
    } else {
      openfiles.front()->open_node(dev);
    }
  } else { 
     openfiles.push_back(new generic_file(dev,descr->grab_exclusive));
     create_inputs(openfiles.back(),openfiles.back()->fds.front(),false);
  }
  return 0;
}

void generic_manager::create_inputs(generic_file* opened_file,int fd, bool watch) {
  if (split == 1) {
    generic_device* gendev = new generic_device(descr->events,fd,watch,slot_man);
    char* newdevname = nullptr;
    asprintf(&newdevname,"%s%d",devname.c_str(),++dev_counter);
    gendev->nameptr = newdevname;
    gendev->name = newdevname;
    opened_file->add_dev(gendev);
    slot_man->request_slot(gendev);
    gendev->start_thread();
    gendev->load_profile(&mapprofile);
  } else {
    for (int i = 1; i <= split; i++) {
      generic_device* gendev = new generic_device(splitevents.at(i-1),fd,watch,slot_man);
      char* newdevname = nullptr;
      asprintf(&newdevname,"%s%d",devname.c_str(),++dev_counter);
      gendev->nameptr = newdevname;
      gendev->name = newdevname;
      opened_file->add_dev(gendev);
      gendev->start_thread();
      gendev->load_profile(&mapprofile);
    }
  }
}
  
void generic_manager::update_maps(const char* evname, event_translator* trans) {
  mapprofile.set_mapping(evname, trans->clone());
  
  for (auto file : openfiles) {
    for (auto dev : file->devices ) {
      dev->update_map(evname, trans);
    }
  }
}
void generic_manager::update_chords(const char* ev1, const char* ev2, event_translator* trans) {
  mapprofile.set_chord(ev1,ev2, trans->clone());
  
  for (auto file : openfiles) {
    for (auto dev : file->devices ) {
      dev->update_chord(ev1,ev2, trans);
    }
  }
}
  
void generic_manager::update_option(const char* opname, const char* value) {
};
  
input_source* generic_manager::find_device(const char* name) {
  
  for (auto file : openfiles) {
    for (auto dev : file->devices ) {
      if (!strcmp(dev->name,name)) return dev;
    }
  }
  return nullptr;
}
enum entry_type generic_manager::entry_type(const char* name) {
  for (auto ev : descr->events) {
    if (!strcmp(ev.name.c_str(),name)) return ev.type;
  }
  return NO_ENTRY;
}