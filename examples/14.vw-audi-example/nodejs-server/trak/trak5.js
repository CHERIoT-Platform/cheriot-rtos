var http = require('http');
var url = require('url');
var Router = require('router');
var finalhandler = require('finalhandler');
var compression  = require('compression')
var bodyParser   = require('body-parser')

var htmlHead = "<HTML><HEAD></HEAD><BODY>";
var htmlBody = "</BODY></HTML>";

async function run() {
  try {
    console.log("Creating server");
    const server = http.createServer(function onRequest(req, res) {
      router(req, res, finalhandler(req, res));
    });
    server.listen(3100);	// Start the server
    console.log("Server running on 3100");
  } catch (err) {
    console.log("ERROR: " + err);
  }
}

run().catch(console.dir);

var buttons = [
  "Mute",
  "Volume Up",
  "Volume Down",
  "Scroll Volume +",
  "Scroll Volume -",
  "Previous",
  "Next",
  "Voice",
  "View",
  "Star",
  "Scroll Up",
  "Scroll Down",
  "Right",
  "Phone",
  "OK",
  "Nav",
  "Down",
  "Up",
  "Back",
  "Left",
  "Drive3",
  "DriveOn!",
  "DriveOff"
];

var router = Router();
router.use(compression())
  .route('/')
  .all(function (req, res, next) {
    req.q = url.parse(req.url, true);
    next();
  })
  .get(function (req, res) {
    console.log("req.method = " + req.method);
    console.log("req.url = " + req.url);
    console.log("req.headers = " + JSON.stringify(req.headers, null, 4));
    console.log("req.q.pathname = " + JSON.stringify(req.q.pathname, null, 4));
    console.log("req.q.query = " + JSON.stringify(req.q.query, null, 4));
    console.log("req.query = " +req.query);
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.write(htmlHead)
    res.write("req.q.pathname = " + req.q.pathname);
    res.write("<br />");
    res.write("req.q.query = " + JSON.stringify(req.q.query));
    res.end(htmlBody);
  });

router.route('/trk/:id')
  .all(function (req, res, next) {
    req.q = url.parse(req.url, true);
    req.q.query.created = new Date();
    next();
  })
  .all(function (req, res, next) {
    //console.log("req.db = " + req.db);
    //console.log("req.params.id = " + req.params.id);
    //console.log("req.method = " + req.method);
    //console.log("req.url = " + req.url);
    //console.log("req.headers = " + JSON.stringify(req.headers, null, 4));
    //console.log("req.q.pathname = " + JSON.stringify(req.q.pathname, null, 4));
    //console.log("req.q.query = " + JSON.stringify(req.q.query, null, 4));
    //console.log("req.q.query.t = " + req.q.query.t);
    //console.log("req.q.query.v = " + req.q.query.v);
    next();
  })
  .get(function(req, res, next) {
    console.log("In the GET");

    var filter = {};  // The DB filter
    if(req.q.query.f && req.q.query.t) {
      filter = {
        "created": {
          $gte: new Date(req.q.query.f),
          $lt: new Date(req.q.query.t)
        }
      };
      console.log("filter: " + JSON.stringify(filter, null, 4));
    }
    res.end();
  })
  .post(function(req, res, next) {
    //console.log("In the POST");

    // TODO! Sanitise the data here. Reject anything too big, anything dangerous, etc.
    console.log(new Date().toISOString() + " POST id: " + req.params.id + " t: " + req.q.query.t + " v: " + buttons[req.q.query.v] + " (" + req.q.query.v  + ")");
    var resJSON = {
      id: req.params.id,
      t: req.q.query.t,
      v: req.q.query.v
    };
    res.end(JSON.stringify(resJSON));
  });
