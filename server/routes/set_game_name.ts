var express = require('express');
var router = express.Router();
var db = require('../model/db');

router.post('/', function (req, res) {
  db.Game.findByIdAndUpdate(req.body.gameId, {
    gameName: req.body.gameName
  }, db.errorHandlerWithResponse(res, (savedGame) => {
    res.status(200);
    res.send(savedGame._id);
  }));
});

module.exports = router;
