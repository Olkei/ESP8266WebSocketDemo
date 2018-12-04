var lastpacket = new Date();

var package = {};
package.connect = function() {
  var ws = new WebSocket("ws://" + document.location.host + "/ws");
  ws.onopen = function() {
    console.log("ws connected");
    document
      .getElementById("status")
      .setAttribute("style", " visibility: hidden;");
    package.ws = ws;
  };
  ws.onerror = function() {
    console.log("ws error");
  };
  ws.onclose = function() {
    console.log("ws closed");
  };
  ws.onmessage = function(msgevent) {
    document
      .getElementById("status")
      .setAttribute("style", " visibility: hidden;");
    lastpacket = new Date();
    try {
      var msg = JSON.parse(msgevent.data);
      console.log("in :", msg);
    } catch (error) {
      console.log(msgevent.data);
    }
  };
};

checkConnection = function() {
  var now = new Date();
  var diff = now.getTime() - lastpacket.getTime();

  if (diff > 5000) {
    document
      .getElementById("status")
      .setAttribute("style", " visibility: visible;");
  }
  
  console.log(diff);
};

package.send = function(msg) {
  if (!this.ws) {
    console.log("no connection");
    return;
  }
  console.log("out:", msg);
  this.ws.send(window.JSON.stringify(msg));
};

package.connect();
