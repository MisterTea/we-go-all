import stripJsonComments from 'strip-json-comments';
import fs from 'fs';

let config = JSON.parse(stripJsonComments(fs.readFileSync("site.config.json", "utf-8")));

module.exports = config;