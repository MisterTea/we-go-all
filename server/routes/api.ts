var express = require('express');
var router = express.Router();
var db = require('../model/db');

const asyncMiddleware = fn =>
  (req, res, next) => {
    Promise.resolve(fn(req, res, next))
      .catch(next);
  };

// Get game id for current user
router.post('/get_current_game_id/:hostkey', asyncMiddleware(async (req, res, _next) => {
  // TODO: authentication

  // TODO: public keys are not unique, need to use id as well
  var hostKey = req.params.hostKey;

  var host = await db.users.findOne({ publicKey: hostKey });
  if (!host) {
    res.status(400);
    res.json({ "error": "Could not find user with key: " + hostKey });
    return;
  }

  var game = await db.games.find({ host: host._id, active: true });
  if (!game) {
    res.status(400);
    res.json({ "error": "Could not find active game with host id: " + host._id });
    return;
  }

  res.status(200);
  res.json({ "gameId": "" + game[0]._id });
}));

router.post('/get_game_info/:gameId', asyncMiddleware(async (req, res, _next) => {
  var gameId = req.params.gameId;

  var game = await db.games.findById(gameId);
  if (!game) {
    res.status(400);
    res.json({ "error": "Could not find game with id: " + game._id });
    return;
  }

  var host = await db.games.findById(game.host._id);
  if (!host) {
    res.status(500);
    res.json({ "error": "Missing host for game " + gameId });
    return;
  }

  var allPeerData = {};
  for (var a = 0; a < game.peers.length; a++) {
    var peerId = game.peers[a];
    var peer = await db.users.findById(peerId);
    if (!peer) {
      res.status(500);
      res.json({ "error": "Missing peer for game: " + gameId + " -> " + peerId });
      return;
    }
    var peerData = {
      "id": peerId,
      "key": peer.publicKey,
      "name": peer.name,
      "endpoints": peer.endpoints,
    };
    allPeerData[peer.publicKey] = peerData;
  }

  res.status(200);
  res.json({ "gameName": game.gameName, "hostKey": host.publicKey, "peerData": allPeerData });
}));


router.post('/host', asyncMiddleware(async (req, res, _next) => {
  var gameId = req.body.gameId;
  var hostKey = req.body.hostKey;
  var gameName = req.body.gameName;

  var game = await db.games.findById(gameId);
  if (!game) {
    res.status(400);
    res.json({ "error": "Could not find game with id: " + gameId });
    return;
  }
  if (game.gameName) {
    res.status(400);
    res.json({ "error": "Game already has name: " + gameId + " -> " + game.gameName });
    return;
  }

  var host = await db.games.findById(game.host._id);
  if (!host) {
    res.status(500);
    res.json({ "error": "Missing host for game " + gameId });
    return;
  }

  if (hostKey != host.publicKey) {
    res.status(400);
    res.json({ "error": "provided key does not match host key: " + hostKey + " != " + host.publicKey });
    return;
  }

  game = await db.games.findByIdAndUpdate({ gameName });
  if (!game) {
    res.status(500);
    res.json({ "error": "Could not update game " + gameId });
    return;
  }

  res.status(200);
  res.send({ "status": "OK" });
}));

module.exports = router;
