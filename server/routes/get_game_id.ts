var express = require('express');
var router = express.Router();
var db = require('../model/db');

router.post('/', function (req, res) {
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

module.exports = router;
