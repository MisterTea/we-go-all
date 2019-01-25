var bunyan = require('bunyan');
var logger = bunyan.createLogger({ name: 'we-go-all-server' });

module.exports = {
  logger: logger,
}