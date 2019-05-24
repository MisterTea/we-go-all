var dgram = require('dgram');
var db = require('../model/db');
var logger = require('./logger').logger;

var HOST = '127.0.0.1';

exports.start = function (port: number) {
  var socket = dgram.createSocket('udp4');

  socket.on('listening', function () {
    var address = socket.address();
    logger.info('UDP Server listening on ' + address.address + ":" + address.port);
  });

  socket.on('message', function (messageObject: object, remote: any) {
    let message = "" + messageObject;
    var tokens = message.split('_');
    if (tokens.length < 1) {
      logger.error("Invalid message: " + message);
      var replyMessage = "ERROR";
      socket.send(replyMessage, 0, replyMessage.length, remote.port, remote.address, function (err: any, bytes: any) {
        if (err) throw err;
        logger.info('UDP message sent to ' + HOST + ':' + port + ' - ' + replyMessage);
      });
      return;
    }
    var playerId = tokens[0];
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

    db.users.findByIdAndUpdate(playerId, { endpoints }, errorHandlerUdp((player: any) => {
      // Update the endpoints
    }));
  });
  socket.bind(port, HOST);

  return socket;
}
