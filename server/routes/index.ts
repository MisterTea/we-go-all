var express = require('express');
var router = express.Router();
import fs from 'fs';

/* GET home page. */
router.get(['/', '/index.html'], function (req, res, next) {
  res.status(200);
  res.send(fs.readFileSync("react_build/index.html"));
});

module.exports = router;
