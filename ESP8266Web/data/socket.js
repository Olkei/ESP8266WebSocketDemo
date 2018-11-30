var package = {};
package.connect = function() {
  var ws = new WebSocket("ws://" + document.location.host + "/ws");
  ws.onopen = function() {
    console.log("ws connected");
    package.ws = ws;
  };
  ws.onerror = function() {
    console.log("ws error");
  };
  ws.onclose = function() {
    console.log("ws closed");
  };
  ws.onmessage = function(msgevent) {
    try {
      var msg = JSON.parse(msgevent.data);
      console.log("in :", msg);
      document.getElementById("red").value = msg.r;
      document.getElementById("green").value = msg.g;
      document.getElementById("blue").value = msg.b;
    } catch (error) {
      console.log(msgevent.data);
    }
    // var msg = msgevent.data;
    // message received, do something
  };
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
