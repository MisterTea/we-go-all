var express = require('express');
var router = express.Router();
var db = require('../model/db');

// Fetch the game state
router.get('/', function (req, res) {
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

module.exports = router;
