var db = require('./db');

export const resolvers = {
  User: {
  },
  Game: {
    async peers(game, _args, _context, _info) {
      var peers: any[] = [];
      for (var peerId in game.peers) {
        peers.push(await db.users.findOneById(peerId));
      }
      return peers;
    },
    async host(game, _args, _context, _info) {
      return await db.users.findOneById(game.host);
    },
  },
  Query: {
    async activeGames() {
      return await db.games.find({ active: true });
    }
  }
};
