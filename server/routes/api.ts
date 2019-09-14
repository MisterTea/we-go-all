var express = require('express');
var router = express.Router();
var ObjectId = require('mongoose').Types.ObjectId;
var db = require('../model/db');

const asyncMiddleware = fn =>
  (req, res, next) => {
    Promise.resolve(fn(req, res, next))
      .catch(next);
  };

router.use(express.json());

// Get game id for current user
router.get('/get_current_game_id/:peerId', asyncMiddleware(async (req, res, _next) => {
  // TODO: authentication

  var peerId = new ObjectId(req.params.peerId);

  console.log(peerId);
  var game = await db.games.find({ peers: { $in: [peerId] }, active: true }).sort('-_id');
  console.log("RESULTS");
  console.log(game);
  console.log(game.length);
  if (game.length == 0) {
    res.status(400);
    res.json({ "error": "Could not find active game with peer id: " + peerId });
    return;
  }
  console.log(game);

  res.status(200);
  res.json({ "gameId": "" + game[0].id });
}));

router.get('/get_game_info/:gameId', asyncMiddleware(async (req, res, _next) => {
  var gameId = new ObjectId(req.params.gameId);

  var game = await db.games.findById(gameId);
  if (!game) {
    res.status(400);
    res.json({ "error": "Could not find game with id: " + game.id });
    return;
  }

  var hasEndpoints = true;
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
      "id": peerId.toString(),
      "key": peer.publicKey,
      "name": peer.name,
      "endpoints": peer.endpoints,
    };
    if (peer.endpoints.length == 0) {
      hasEndpoints = false;
    }
    console.log(JSON.stringify(peerData));
    allPeerData[peerId.toString()] = peerData;
  }

  res.status(200);
  res.json({
    gameName: game.gameName,
    ready: (game.ready.length == game.peers.length) && hasEndpoints,
    hostId: game.host.toString(),
    peerData: allPeerData,
  });
}));


router.post('/host', asyncMiddleware(async (req, res, _next) => {
  console.log("HOST COMMAND");
  console.log(req);
  console.log(req.body);
  var gameId = new ObjectId(req.body.gameId);
  var hostId = new ObjectId(req.body.hostId);
  var gameName = req.body.gameName;
  console.log(gameId);

  var game = await db.games.findById(gameId);
  if (!game) {
    res.status(400);
    res.json({ "error": "Could not find game with id: " + gameId });
    return;
  }
  if (game.gameName && game.gameName != gameName) {
    res.status(400);
    res.json({ "error": "Game already has name: " + gameId + " -> " + game.gameName });
    return;
  }
  game.gameName = gameName;
  var found = false;
  for (var a = 0; a < game.ready.length; a++) {
    if (game.ready[a].equals(hostId)) {
      found = true;
    }
  }
  if (!found) {
    game.ready.push(hostId);
  }
  await game.save();

  res.status(200);
  res.send({ "status": "OK" });
}));

router.post('/join', asyncMiddleware(async (req, res, _next) => {
  var peerId = new ObjectId(req.body.peerId);
  var name = req.body.name;
  var peerKey = req.body.peerKey;
  console.log("Peer id");

  var gameList = await db.games.find({ peers: { $in: new ObjectId(peerId) }, active: true });
  if (gameList.length == 0) {
    res.status(400);
    res.json({ "error": "Could not find active game with host id: " + peerId });
    return;
  }
  var game = gameList[0];
  console.log(game);
  var found = false;
  for (var a = 0; a < game.ready.length; a++) {
    if (game.ready[a].equals(peerId)) {
      found = true;
    }
  }
  if (!found) {
    game.ready.push(peerId);
  }
  await game.save();

  res.status(200);
  res.send({ "status": "OK" });
}));

module.exports = router;
