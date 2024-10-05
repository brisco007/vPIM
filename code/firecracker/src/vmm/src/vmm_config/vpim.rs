// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

use std::collections::VecDeque;
use std::fmt;
use std::sync::{Arc, Mutex};
use devices::virtio::vpim::Error as VPIMDeviceError;
pub use devices::virtio::VPIM_DEV_ID;
use devices::virtio::{VPIMDevice, VPIMDeviceConfig};

use serde::{Deserialize, Serialize};

type MutexVPIMDevice = Arc<Mutex<VPIMDevice>>;

/// Errors associated with the operations allowed on the vpim device.
#[derive(Debug)]
pub enum VPIMDeviceConfigError {
    /// The user made a request on an inexistent vpim device.
    DeviceNotFound,
    /// Device not activated yet.
    DeviceNotActive,
    /// Failed to create a vpim device.
    CreateFailure(devices::virtio::vpim::Error),
    /// Failed to update the configuration of the vpim device.
    UpdateFailure(std::io::Error),
}
//TODO fill in all the required errors texts here
impl fmt::Display for VPIMDeviceConfigError {
    fn fmt(&self, f: &mut fmt::Formatter) -> std::fmt::Result {
        use self::VPIMDeviceConfigError::*;
        match self {
            DeviceNotFound => write!(f, "No VPIM device found."),
            DeviceNotActive => write!(
                f,
                "Device is inactive, check if VPIM driver is enabled in guest kernel."
            ),
            CreateFailure(e) => write!(f, "Error creating the VPIM device: {:?}", e),
            UpdateFailure(e) => write!(
                f,
                "Error updating the VPIM device configuration: {:?}",
                e
            ),
        }
    }
}

impl From<VPIMDeviceError> for VPIMDeviceConfigError {
    fn from(error: VPIMDeviceError) -> Self {
        match error {
            VPIMDeviceError::DeviceNotFound => Self::DeviceNotFound,
            VPIMDeviceError::DeviceNotActive => Self::DeviceNotActive,
            VPIMDeviceError::InterruptError(io_error) => Self::UpdateFailure(io_error),
            e => Self::CreateFailure(e),
        }
    }
}

type Result<T> = std::result::Result<T, VPIMDeviceConfigError>;

/// This struct represents the strongly typed equivalent of the json body
/// from VPIM related requests.
#[derive(Clone, Debug, Default, PartialEq,Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VPIMDeviceConfigFromAPI {
    ///This used to lock a rank until shutdown
    pub hold_rank:u8
}

impl From<VPIMDeviceConfig> for VPIMDeviceConfigFromAPI {
    fn from(state: VPIMDeviceConfig) -> Self {
        VPIMDeviceConfigFromAPI {
            hold_rank : state.hold_rank
        }
    }
}

/// The data fed into a vpim update request.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VPIMDeviceUpdateConfig {
    ///This used to lock a rank until shutdown
    pub hold_rank:u8
}

#[derive(Default)]
/// A builder for `VPIMDevice` devices from 'VPIMDeviceConfigFromAPI'.
pub struct VPIMDeviceBuilder {
    //inner: Option<MutexVPIMDevice>,
    ///List of VPIM devices
    pub list: VecDeque<MutexVPIMDevice>
}

/* #[cfg(not(test))]
impl Default for VPIMDeviceBuilder {
    fn default() -> VPIMDeviceBuilder {
        VPIMDeviceBuilder { inner: None }
    }
} */

impl VPIMDeviceBuilder {
     /// Constructor for vPIM devices. It initializes an empty LinkedList.
     pub fn new() -> Self {
        Self {
            list: VecDeque::<MutexVPIMDevice>::new(),
        }
    }


      /// Inserts an existing  device.
      pub fn add_device(&mut self, vpim_device: MutexVPIMDevice) {
        self.list.push_back(vpim_device);

    }

     /// Inserts a device in the devices list using the specified configuration.
    /// If a device with the same id already exists, it will overwrite it.
    /// Inserting a secondary root block device will fail.
    pub fn insert(&mut self, config: VPIMDeviceConfigFromAPI) -> Result<()> {

        let vpim_dev = Arc::new(Mutex::new(Self::create_vpim_device(config)?));
/*         println!("before");
        spawn_thread(vpim_dev.clone());
        println!("after"); */
        // If the id of the drive already exists in the list, the operation is update/overwrite.
        self.list.push_back(vpim_dev);
        //TODO the manager should have an access to this list. 
        Ok(())
    }
    ///Creates a new device 
    pub fn create_vpim_device( vpim_config:VPIMDeviceConfigFromAPI ) -> Result<VPIMDevice> {
        VPIMDevice::new(vpim_config.hold_rank)
        .map_err(VPIMDeviceConfigError::CreateFailure)
    }
    // Inserts a VPIM device in the store.
    // If an entry already exists, it will overwrite it.
/*     pub fn set(&mut self, cfg: VPIMDeviceConfigFromAPI) -> Result<()> {
        self.inner = Some(Arc::new(Mutex::new(
            VPIMDevice::new(
                cfg.vm_in,
                cfg.safe_mode,
                cfg.base_addr,
                cfg.size,
            )
            .map_err(VPIMDeviceConfigError::CreateFailure)?,
        )));

        Ok(())
    } */

   /*  /// Inserts an existing VPIM device.
    pub fn set_device(&mut self, vpim: MutexVPIMDevice) {
        self.inner = Some(vpim);
    }
 */
    // Provides a reference to the VPIM device if present.
    /* pub fn get(&self,id: u8) -> Option<&MutexVPIMDevice> {
        let mut result : Option<&MutexVPIMDevice> = None;
        for i in self.list {
            if i.lock().unwrap().get_id() == id {
                result = Some(&i);
                break;
            }
        }
        result
    } */
    /// Returns a vec with the structures used to configure the devices.
    pub fn configs(&self) -> Vec<VPIMDeviceConfigFromAPI> {
        let mut ret = vec![];
        for vpim in &self.list {
            ret.push(VPIMDeviceConfigFromAPI::from(vpim.lock().unwrap().config()));
        }
        ret
    }
   /*  pub fn get_config(&self) -> Result<VPIMDeviceConfigFromAPI> {
        self.get()
            .ok_or(VPIMDeviceConfigError::DeviceNotFound)
            .map(|vpim_mutex| vpim_mutex.lock().expect("Poisoned lock").config())
            .map(VPIMDeviceConfigFromAPI::from)
    } */
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    pub(crate) fn default_config() -> VPIMDeviceConfigFromAPI {
        VPIMDeviceConfigFromAPI {
        }
    }

    impl Default for VPIMDeviceBuilder {
        fn default() -> VPIMDeviceBuilder {
            let mut vpim = VPIMDeviceBuilder::new();
            assert!(vpim.set(VPIMDeviceConfig::default()).is_ok());
            vpim
        }
    }

 /*    #[test]
    fn test_vpim_create() {
        let default_vpim_config = default_config();
        let vpim_config = VPIMDeviceConfig {
            vm_in: 0,
            base_addr: 16,
            size: 60,
            safe_mode: true,
        };
        assert_eq!(default_vpim_config, vpim_config);
        let mut builder = VPIMDeviceBuilder::new();
        assert!(builder.get().is_none());

        builder.set(vpim_config).unwrap();
        assert_eq!(builder.get().unwrap().lock().unwrap().base_addr(), 16);
        println!("{:?}", builder.get_config().unwrap());
        println!("{:?}", default_vpim_config);
        assert_eq!(builder.get_config().unwrap(), default_vpim_config);

        let _update_config = VPIMDeviceUpdateConfig { safe_mode: true };
    }
 */
    #[test]
    fn test_from_vpim_state() {
        let expected_vpim_config = VPIMDeviceConfigFromAPI  {
        };

        let actual_vpim_config = VPIMDeviceConfigFromAPI::from(VPIMDeviceConfig  {
        });

        assert_eq!(expected_vpim_config, actual_vpim_config);
    }

    #[test]
    fn test_error_messages() {
        use super::VPIMDeviceConfigError::*;
        use std::io;
        let err = CreateFailure(devices::virtio::vpim::Error::EventFd(
            io::Error::from_raw_os_error(0),
        ));
        let _ = format!("{}{:?}", err, err);

        let err = UpdateFailure(io::Error::from_raw_os_error(0));
        let _ = format!("{}{:?}", err, err);

        let err = DeviceNotFound;
        let _ = format!("{}{:?}", err, err);
    }

/*     #[test]
    fn test_set_device() {
        let mut builder = VPIMDeviceBuilder::new();
        let vpim = VPIMDevice::new(1, true, 16, 8).unwrap();
        builder.set_device(Arc::new(Mutex::new(vpim)));
        assert!(builder.inner.is_some());
    } */
}
