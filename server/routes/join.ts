var express = require('express');
var router = express.Router();
var db = require('../model/db');

router.post('/', function (req, res) {
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

module.exports = router;
