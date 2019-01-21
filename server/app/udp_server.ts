var dgram = require('dgram');
var db = require('../model/db');
var logger = require('./logger').logger;

var PORT = 3000;
var HOST = '127.0.0.1';

var socket = dgram.createSocket('udp4');

socket.on('listening', function () {
  var address = socket.address();
  logger.info('UDP Server listening on ' + address.address + ":" + address.port);
});

socket.on('message', function (message: string, remote: any) {
  var tokens = message.split('_');
  if (tokens.length < 2) {
    logger.error("Invalid message: " + message);
    var replyMessage = "ERROR";
    socket.send(replyMessage, 0, replyMessage.length, remote.port, remote.address, function (err: any, bytes: any) {
      if (err) throw err;
      logger.info('UDP message sent to ' + HOST + ':' + PORT + ' - ' + replyMessage);
    });
    return;
  }
  var playerKey = tokens[0];
  var endpoints: string[] = [];
  endpoints.push(remote.address + ':' + remote.port);
  for (var a = 1; a < tokens.length; a++) {
    endpoints.push(tokens[a]);
  }

  var errorHandlerUdp = function (callback: any) {
    return function (err: any, o: any) {
      if (err) {
        logger.error(err);
        return;
      }
      callback(o);
    }
  };

  db.player.FindOneAndUpdate({ publicKey: playerKey }, errorHandlerUdp((player: any) => {
    // Update the endpoints
  }));
});

exports.start = function () {
  socket.bind(PORT, HOST);
}
