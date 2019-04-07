var express = require('express');
var router = express.Router();
var db = require('../model/db');

router.post('/host', function (req, res) {
  var game = new db.Game({
    gameName: "",
    host: req.body.publicKey,
    createTime: (new Date).getTime(),
    active: true,
    players: [
      { publicKey: req.body.publicKey, name: req.body.playerName }
    ]
  });
  game.save(db.errorHandlerWithResponse(res, (savedGame) => {
    if (savedGame) {
      res.status(200);
      res.send(savedGame._id);
    } else {
      console.error("Could not save game");
      res.status(500);
      res.send("Error: Could not save game");
    }
  }));
});

// Fetch the game state
router.get('/gameinfo', function (req, res) {
  var game_id = req.param('id');
  db.Game.findById(game_id, db.errorHandlerWithResponse(res, (game) => {
    if (game == null) {
      res.status(400);
      res.send(JSON.stringify({ error: "Invalid game id: " + game_id }));
      return;
    }
    res.status(200);
    res.send(JSON.stringify(game));
    return;
  }))
});

router.post('/get_game_id', function (req, res) {
  db.Game.findOne({ players: req.body.publicKey, active: true }).sort('-createTime').exec(
    db.errorHandlerWithResponse(res, (game) => {
      if (game == null) {
        res.status(400);
        res.send("Invalid request: No games found");
        return;
      }

      res.status(200);
      res.send(JSON.stringify({ gameId: game._id }));
    }));
});

router.post('/join', function (req, res) {
  db.Game.findOne({ host: req.body.host, active: true }).sort('-createTime').exec(
    db.errorHandlerWithResponse(res, (game) => {
      if (game == null) {
        res.status(400);
        res.send("Invalid request: No games found");
        return;
      }

      var newPlayer = {
        publicKey: req.body.publicKey,
        name: req.body.name,
      };

      var foundPlayer = null;
      game.players.forEach(player => {
        if (player.publicKey === newPlayer.publicKey) {
          foundPlayer = player;
        }
      });
      if (foundPlayer != null) {
        res.status(400);
        res.send(JSON.stringify({ error: "Tried to join a game that you already joined" }));
        return;
      }

      game.players.push(newPlayer);
      game.save(db.errorHandlerWithResponse(res, (savedGame) => {
        if (savedGame) {
          res.status(200);
          res.send(JSON.stringify(savedGame));
        } else {
          console.error("Could not save game");
          res.status(500);
          res.send("Error: Could not save game");
        }
      }));
    }));
});

router.post('/set_game_name', function (req, res) {
  db.Game.findByIdAndUpdate(req.body.gameId, {
    gameName: req.body.gameName
  }, db.errorHandlerWithResponse(res, (savedGame) => {
    res.status(200);
    res.send(savedGame._id);
  }));
});

router.post('/set_user_info', function (req, res) {
  console.log("SETTING USER INFO");
  db.users.findByIdAndUpdate(req.cookies["user_id"], { publicKey: req.body.publicKey }, db.errorHandlerWithResponse(res, (savedUser) => {
    console.log("DONE");
    res.status(200);
    res.send(savedUser._id.toString());
  }));
});

module.exports = router;
