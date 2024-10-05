// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

use super::super::VmmAction;
use crate::parsed_request::{Error, ParsedRequest};
use crate::request::Body;
/* use micro_http::StatusCode;
 */use vmm::vmm_config::vpim::{
    VPIMDeviceConfigFromAPI, VPIMDeviceUpdateConfig,
};

/// This function is used to create a vPIM device
pub(crate) fn parse_put_vpim(/**/ body: &Body ) -> Result<ParsedRequest, Error> {
    Ok(ParsedRequest::new_sync(VmmAction::InsertVPIMDevice(
        serde_json::from_slice::<VPIMDeviceConfigFromAPI>(body.raw()).map_err(Error::SerdeJson)?,
    )))
}

///THis function is used to modify the vPIM device
pub(crate) fn parse_patch_vpim(
     body: &Body,
   /* path_second_token: Option<&&str>, */
) -> Result<ParsedRequest, Error> {
    Ok(ParsedRequest::new_sync(VmmAction::UpdateVPIMDevice(
        serde_json::from_slice::<VPIMDeviceUpdateConfig>(body.raw()).map_err(Error::SerdeJson)?,
    )))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::parsed_request::tests::vmm_action_from_request;

    #[test]
    fn test_parse_get_vpim_request() {
        assert!(parse_get_vpim(/* None */).is_ok());
    }

    #[test]
    fn test_parse_patch_vpim_request() {
        assert!(parse_patch_vpim( &Body::new("invalid_payload")/*, None */).is_err());

        // PATCH with invalid fields.
        let body = r#"{
             "vm_in": "foo",
             "base_addr" : "ddgdgdgdg",
             "size": "fefefefe",
             "safe_mode": 123,
              }"#;
        assert!(parse_patch_vpim( &Body::new(body)).is_err());

        // PATCH with invalid types on fields. Adding safe_mode as string instead of bool.
        let body = r#"{
            "vm_in": 15,
            "base_addr" : 0,
            "size": 128,
            "safe_mode": "true",
             }"#;
        let res = parse_patch_vpim( &Body::new(body));
        assert!(res.is_err());

        // PATCH with invalid types on fields. Adding a size as a negative number.
        let body = r#"{
            "vm_in": 15,
            "base_addr" : 0,
            "size": -128,
            "safe_mode": "true",
             }"#;
        let res = parse_patch_vpim( &Body::new(body)/*, None */);
        assert!(res.is_err());

        // PATCH on statistics with missing safe_mode field.
        let body = r#"{
            "vm_in": 15,
            "base_addr" : 0,
            "size": -128,
             }"#;
        let res = parse_patch_vpim( &Body::new(body)/*, Some(&"statistics") */);
        assert!(res.is_err());

        // PATCH with payload that is not a json.
        let body = r#"{
                "fields": "dummy_field"
              }"#;
        assert!(parse_patch_vpim(&Body::new(body)/* , None */).is_err());

    
        let body = r#"{
            "safe_mode": true 
             }"#;

        #[allow(clippy::match_wild_err_arm)]
        match vmm_action_from_request(parse_patch_vpim( &Body::new(body)/*, None */).unwrap()) {
            VmmAction::UpdateVPIMDevice(vpim_cfg) => assert_eq!(vpim_cfg.safe_mode, true),
            _ => panic!("Test failed: Invalid parameters"),
        }; 
      
    }

    #[test]
    fn test_parse_put_vpim_request() {
        assert!(parse_put_vpim( &Body::new("invalid_payload") /**/).is_err());

        // PUT with invalid fields.
        let body = r#"{
                "amount_mib": "bar",
                "is_read_only": false
              }"#;
        assert!( parse_put_vpim(&Body::new(body) /**/).is_err());

        // PUT with valid input fields.
        let body = r#"{
                "amount_mib": 1000,
                "deflate_on_oom": true,
                "stats_polling_interval_s": 0
            }"#;
        assert!(parse_put_vpim  (&Body::new(body)/**/).is_ok());
    }
}
