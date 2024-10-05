#![allow(nonstandard_style)]
#![allow(dead_code)]
#![allow(unused_imports)]
#![allow(unused_assignments)]
#![allow(unused_variables)]
#![allow(improper_ctypes)]
use std::os::unix::net::UnixStream;
 use std::io::{self, Write, Read};


pub const LINK: &str = "/tmp/pim_mgr.sock";
pub const CLIENT_PATH: &str = "/tmp/unix_sock.client";
pub const MSG_MAX_SIZE: usize = 1024;
pub const VPIM_DEFAULT_ID:u32 = 99;

//#[link(name = "client_zmq")]


pub const REQ_ALLOC:u32 = 0;
pub const REQ_FREE:u32 = 1;

pub const RES_OK:u32 = 0;
pub const RES_FAILED:u32 = 1;
pub const RES_UNAVAILABLE:u32 = 2;

#[repr(C)]
pub enum ReqType {
    ReqAlloc = 0,
    ReqFree = 1,
}

#[repr(C)]
pub enum ResponseStatus {
    ResOk = 0,
    ResFailed = 1,
    ResUnavailable = 2,
}

#[repr(C)]
#[derive(Clone, Default, Eq, PartialEq)]
struct Entry {
    rank_path: String,
    dax_path: String,
    is_owned: i32,
    vpim_id: u32,
    rank_id: i32,
}

#[repr(C)]
#[derive(Clone,Debug, Default, Eq, PartialEq)]
pub struct Request {
    pub req_type: u32,
    pub req_len: u32,
    pub vpim_id: u32,
}

#[repr(C)]
#[derive(Clone,Debug, Default, Eq, PartialEq)]
pub struct Response {
    pub req_type: u32,
    pub status: u32,
    pub req_len: u32,
    pub vpim_id: u32,
    pub rank_path: String,
    pub dax_path: String,
    pub rank_id: u32,
}


impl Request {
    ///Serialize the request to be sent to the manager
    pub fn serialize(&self) -> String {
        format!(
            "{},{},{}",
            self.req_type,
            self.req_len,
            self.vpim_id,
        )
    }
    ///Deserialize the request string
    pub fn deserialize(serialized_req: &str) -> Option<Request> {
        let mut split = serialized_req.split(',');

        let req_type = split.next()?.parse::<u32>().ok()?;
        let req_len = split.next()?.parse::<u32>().ok()?;
        let vpim_id = split.next()?.parse::<u32>().ok()?;

        Some(Request {
            req_type,
            req_len,
            vpim_id
        })
    }
}


impl Response {
    ///Serialize the repsonse struct
    pub fn serialize(&self) -> String {
        format!(
            "{},{},{},{},{},{},{}",
            self.req_type,
            self.status,
            self.req_len,
            self.vpim_id,
            self.rank_path,
            self.dax_path,
            self.rank_id
        )
    }
    ///Deserialize the response string that comes from the manager
    pub fn deserialize(serialized_response: &str) -> Option<Response> {
        let split: Vec<&str> = serialized_response.trim().split(",").collect();
        if split.len() < 7 {
            return None;
        }
    
        let req_type = split[0].parse::<u32>().ok()?;
        let status = split[1].parse::<u32>().ok()?;
        let req_len = split[2].parse::<u32>().ok()?;
        let vpim_id = split[3].parse::<u32>().ok()?;
        let rank_path = split[4].to_string();
        let dax_path = split[5].to_string();
        let rank_id = split[6].parse::<u32>().ok()?;
    
        let output = Some(Response {
            req_type,
            status,
            req_len,
            vpim_id,
            rank_path,
            dax_path,
            rank_id,
        });
    
        output
    }
}

///Structure that is used to communicate ith the manager
pub struct Manager {
   pub vpim_id: u32
}

impl Manager {
    pub fn new() -> Result<Self, String> {
        Ok(Manager {
            vpim_id: VPIM_DEFAULT_ID,
        })
    }
    pub fn is_vpim_id(&self) -> bool{
        !(self.vpim_id == VPIM_DEFAULT_ID)
    }
    pub fn send_req(&mut self, request: &Request) -> Option<Response> {
        let mut stream = UnixStream::connect(LINK).ok()?;
        let serialized_request = request.serialize();
        let mut response = String::new();
        let mut buffer: [u8;MSG_MAX_SIZE] = [0;MSG_MAX_SIZE];
        
        stream.write_all(serialized_request.as_bytes()).ok()?;
        stream.flush().ok()?;
        let size = stream.read(&mut buffer);
        match size {
            Ok(res_size) => {  
                let resp = String::from_utf8(buffer.to_vec()).ok().unwrap();
                response = resp.trim().split("$").collect::<Vec<&str>>()[0].to_string();
                let res = Response::deserialize(response.as_str());
                res
            },
            Err(e) => {
                println!("{}",e);
                None
            },
        }
        
    }
}

