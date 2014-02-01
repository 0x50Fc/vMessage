

function vMessageClient(url,username,password){
    this.url = url;
    this.recvHttp = null;
    this.timestamp = "0";
    this.username = username;
    this.password = password;
    this.idle = 300;
}

vMessageClient.prototype = {

    onmessage : function (msg) {
    
    },
    onerror : function(http) {
    
    },
    start : function() {
        if(this.recvHttp == null){
            
            this.started = true;
            
            var url = String(this.url);
            
            if(url.length >0){
                
                var http ;
                
                try {
                    http = new XMLHttpRequest();
                } catch(e){
                    http = new ActiveXObject("Microsoft.XMLHTTP");
                }
                
                var lChar = url.substr(url.length - 1,0);
                
                if(lChar == "/"){
                    url = url + this.timestamp;
                }
                else{
                    url = url + "/" + this.timestamp;
                }
                                
                http.open("GET",url,true,this.username,this.password);
                
                if(this.username){
                    http.setRequestHeader("Authorization","Basic "+btoa(this.username + ":" + this.password));
                }
                
                this.recvHttp = http;
                
                var self = this;
                var index = 0;
                var state = 0;
                var key = [];
                var value = [];
                var body = [];
                var header = {};
                var length = 0;
                var hasMessage = false;
                
                var readFn = function(){
                    
                    var text = http.responseText;
                    var len = text.length;
                    
                    for(;index <len;index ++){
                        
                        var char = text.substr(index,1);
                        
                        if(state == 0){
                            
                            if(char == ':'){
                                state = 1;
                            }
                            else if(char == '\r'){
                                
                            }
                            else if(char == '\n'){
                                state = 2;
                                length = parseInt( header["Content-Length"] );
                            }
                            else{
                                key.push(char);
                            }
                            
                        }
                        else if(state == 1){
                            
                            if(value.length == 0){
                                if(char == ' '){
                                    
                                }
                                else if(char == '\r'){
                                    
                                }
                                else if(char == '\n'){
                                    
                                    if(key.length){
                                        header[key.join('')] = value.join('');
                                        key = [];
                                        value = [];
                                    }
                                    
                                    state = 0;
                                }
                                else{
                                    value.push(char);
                                }
                            }
                            else{
                                if(char == '\r'){
                                    
                                }
                                else if(char == '\n'){
                                    
                                    if(key.length){
                                        header[key.join('')] = value.join('');
                                        key = [];
                                        value = [];
                                    }
                                    
                                    state = 0;
                                }
                                else{
                                    value.push(char);
                                }
                            }
                            
                        }
                        else if(state == 2){
                            
                            if(length == NaN || length == 0){
                                if(char == '\r'){
                                    
                                }
                                else if(char == '\n'){
                                    
                                    var timestamp = header["Timestamp"];
                                    
                                    if(timestamp){
                                        self.timestamp = timestamp;
                                    }
                                    
                                    var ctn = body.join('');
                                    var type = header["Content-Type"];
                                    
                                    if(type == "application/x-www-form-urlencoded"){
                                        var b = {};
                                        var items = ctn.split("&");
                                        for(var ii=0;ii<items.length;ii++){
                                            var kv = items[ii].split("=");
                                            if(kv.length ==2){
                                                b[kv[0]] = unescape(kv[1]);
                                            }
                                        }
                                        ctn = b;
                                    }
                                    
                                    var msg = {
                                        "header":header,
                                        "body":ctn
                                    };
                                    
                                    hasMessage = true;
                                    
                                    self.onmessage(msg);
                                    
                                    state = 0;
                                    key = [];
                                    value = [];
                                    body = [];
                                    length = 0;
                                }
                                else{
                                    state = 0;
                                    key = [];
                                    value = [];
                                    body = [];
                                    length = 0;
                                    break;
                                }
                            }
                            else{
                                body.push(char);
                                length --;
                            }
                        }
                    }
                };
                
                http.onreadystatechange = function (){
                    if(http.readyState == 3){
                        readFn();
                    }
                    else if(http.readyState == 4){
                        if(http.status != 200){
                            self.onerror(http);
                            self.recvHttp = null;
                            if(self.started){
                                //window.setTimeout( function(){ self.start(); } , 300 );
                            }
                        }
                        else{
                            
                            var tm = http.getResponseHeader("Timestamp");
                            
                            if(tm && self.timestamp == "0"){
                                self.timestamp = tm;
                            }
                            
                            self.recvHttp = null;
                            if(self.started){
                                
                                readFn();
                                
                                if(self.startTimeout){
                                    window.clearTimeout(self.startTimeout);
                                }
                                
                                if(hasMessage){
                                    self.idle = 300;
                                }
                                
                                self.startTimeout = window.setTimeout( function(){ if(self.started) {self.start();} } , self.idle);
                                self.idle += 300;
                                if(self.idle > 6000){
                                    self.idle = 6000;
                                }
                            }
                        }
                    }
                };
                
                this.recvHttp.send();
            }
            
        }
    },
    
    stop : function() {
        this.started = false;
        if(this.recvHttp){
            this.recvHttp.abort();
            this.recvHttp = null;
        }
    },
    
    send : function(to,msg,fn) {
        
        var self = this;
        
        this.idle = 300;
        
        if(this.started){
            if(this.startTimeout){
                window.clearTimeout(this.startTimeout);
            }
            this.startTimeout = window.setTimeout( function(){ self.start(); } , this.idle);
        }
        
        var url = String(this.url);
        
        if(url.length >0 && to){
            
            var http ;
            
            try {
                http = new XMLHttpRequest();
            } catch(e){
                http = new ActiveXObject("Microsoft.XMLHTTP");
            }
            
            var lChar = url.substr(url.length - 1,0);
            
            if(lChar == "/"){
                url = url + to;
            }
            else{
                url = url + "/" + to;
            }
            
            var body = [];
            
            for(var key in msg){
                body.push("&");
                body.push(key);
                body.push("=");
                body.push(escape(msg[key]));
            }
            
            var self = this;
            
            http.open("POST",url,true,this.username,this.password);
            
            if(this.username){
                http.setRequestHeader("Authorization","Basic "+btoa(this.username + ":" + this.password));
            }
            
            http.setRequestHeader("Content-Type","application/x-www-form-urlencoded");
            
            http.onreadystatechange = function (){
                if(http.readyState == 4){
                    if(http.status != 200){
                        self.onerror(http);
                    }
                    else if(fn){
                        fn(http);
                    }
                }
            };
            
            http.send(body.join(''));
        }
    },
};

