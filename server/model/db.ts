var mongoose = require('mongoose');
var config = require('../server/config');
var ObjectId = mongoose.Schema.Types.ObjectId;

var UserSchema = new mongoose.Schema({
  email: {
    type: String,
    index: true,
    unique: true,
    required: true,
  },
  discordId: {
    type: String,
    index: true,
    unique: true,
  },
  githubId: {
    type: String,
    index: true,
    unique: true,
  },
  name: {
    type: String,
    index: true,
    unique: true,
  },
  password: String,
  publicKey: {
    type: String,
    index: true,
    unique: true,
  },
  endpoints: [String],
});
exports.users = mongoose.model('User', UserSchema);

var GameSchema = new mongoose.Schema({
  gameName: String,
  host: ObjectId,
  peers: [ObjectId],
  active: Boolean,
});
exports.games = mongoose.model('Game', GameSchema);

exports.start = function (callback) {
  console.log("Connecting to database");
  mongoose.connect(config.db.connectString, { useNewUrlParser: true }, callback);
};

exports.errorHandler = function (callback) {
  return function (err, retval) {
    var args = Array.from(arguments);
    if (args[0]) {
      console.error(err);
      return;
    }
    callback(retval);
  };
}

exports.errorHandlerWithResponse = function (res, callback) {
  return function (err, retval) {
    var args = Array.from(arguments);
    if (args[0]) {
      console.error(err);
      res.status(500);
      res.send('Server error');
      return;
    }
    callback(retval);
  };
}