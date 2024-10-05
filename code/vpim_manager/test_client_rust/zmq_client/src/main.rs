 use std::io::{self, Write, Read};
 use std::net::TcpStream;
 
 struct Request {
    id: u32,
    data: String,
}


fn main() -> io::Result<()> {
    let request_msg = format!("{},{},{}", 0, 0, 99);
    let request = Request {
        id: 1,
        data: request_msg,
    };
    for _ in 0..10 {
        let mut stream = TcpStream::connect("127.0.0.1:8080")?;
        //let mut reader = BufReader::new(&stream);
        let serialized_request = request.data.clone();
        let mut response = String::new();
        let mut buffer: [u8;1024] = [0;1024];
        
        stream.write_all(serialized_request.as_bytes())?;
        stream.flush()?;
        println!("request");
        let size = stream.read(&mut buffer);
        match size {
            Ok(res_size) => {
                response = String::from_utf8(buffer.to_vec()).ok().unwrap();
            },
            Err(e) => {println!("{}",e)},
        }
        println!("Received response: {}", response.trim());
    }

    Ok(())
}
