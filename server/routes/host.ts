var express = require('express');
var router = express.Router();
var db = require('../model/db');

router.post('/', function (req, res) {
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

module.exports = router;
