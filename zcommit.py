#!/usr/bin/python

import cherrypy
from flup.server.fcgi import WSGIServer
import logging
import json
import os
import subprocess
import sys
import traceback
import dateutil.parser

HERE = os.path.abspath(os.path.dirname(__file__))
ZWRITE = os.path.join(HERE, 'bin', 'zsend')
LOG_FILENAME = 'logs/zcommit.log'

# Set up a specific logger with our desired output level
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Add the log message handler to the logger
handler = logging.FileHandler(LOG_FILENAME)
logger.addHandler(handler)

def zephyr(sender, klass, instance, zsig, msg):
    # TODO: spoof the sender
    logger.info("""About to send zephyr:
sender: %(sender)s
class: %(klass)s
instance: %(instance)s
zsig: %(zsig)s
msg: %(msg)s""" % {'sender' : sender,
                   'klass' : klass,
                   'instance' : instance,
                   'zsig' : zsig,
                   'msg' : msg})
    cmd = [ZWRITE, '-S', sender, '-c', klass, '-i', instance,
           '-s', zsig, '-d', '-m', msg]
    subprocess.check_call([p.encode('utf-8') for p in cmd])

class Application(object):
    @cherrypy.expose
    def index(self):
        logger.debug('Hello world app reached')
        return """
<p> <i>Welcome to zcommit.</i> </p>

<p> zcommit allows you to send zephyr notifications by sending an HTTP
POST request to a URL.  Currently zcommit supports POST-backs from
github.  If you would like it to support another form of POST-back,
please let us know (zcommit@mit.edu). </p>

<h1> URL structure </h1>

The URL you post to is structured as follows:
<tt>http://zcommit.mit.edu/$type/$key1/$value1/$key2/$value2/...</tt>.
So for example, the URL
<tt>http://zcommit.mit.edu/github/class/zcommit/instance/commit</tt>
is parsed as having type <tt>github</tt>, class <tt>zcommit</tt>, and
instance <tt>commit</tt>.  Using this information, zcommit figures out
how to form a useful message which is then sends as a zephyr.

<h1> Types </h1>

<h2> Github </h2>

Set your POST-back URL to
<tt>http://zcommit.mit.edu/github/class/$classname</tt>, followed by
any of the following optional key/value parameters:

<ul>
<li> <tt>/instance/$instance</tt> </li>
<li> <tt>/zsig/$zsig</tt> (sets the prefix of the zsig; the postfix is always the branch name) </li>
<li> <tt>/sender/$sender</tt> </li>
</ul>

<h2> Default </h2>

Same as github, but with <tt>github</tt> replaced by <tt>default</tt>
in the URL, and no <tt>sender</tt> parameter. The <tt>payload</tt>
parameter of the POST should contain the body of the zephyr.
"""

    class Github(object):
        @cherrypy.expose
        def default(self, *args, **query):
            try:
                return self._default(*args, **query)
            except Exception, e:
                logger.error('Caught exception %s:\n%s' % (e, traceback.format_exc()))
                raise

        def _default(self, *args, **query):
            logger.info('A %s request with args: %r and query: %r' %
                        (cherrypy.request.method, args, query))
            opts = {}
            if len(args) % 2:
                raise cherrypy.HTTPError(400, 'Invalid submission URL')
            logger.debug('Passed validation')
            for i in xrange(0, len(args), 2):
                opts[args[i]] = unicode(args[i + 1], 'utf-8', 'replace')
            logger.debug('Set opts')
            if 'class' not in opts:
                raise cherrypy.HTTPError(400, 'Must specify a zephyr class name')
            logger.debug('Specified a class')
            if cherrypy.request.method == 'POST':
                logger.debug('About to load data')
                payload = json.loads(query['payload'])
                logger.debug('Loaded payload data')
                zsig = payload['ref']
                if 'zsig' in opts:
                    zsig = '%s: %s' % (opts['zsig'], zsig)
                sender = opts.get('sender', 'daemon.zcommit')
                logger.debug('Set zsig')
                for c in payload['commits']:
                    inst = opts.get('instance', c['id'][:8])
                    actions = []
                    if c.get('added'):
                        actions.extend('  A %s\n' % f for f in c['added'])
                    if c.get('removed'):
                        actions.extend('  D %s\n' % f for f in c['removed'])
                    if c.get('modified'):
                        actions.extend('  M %s\n' % f for f in c['modified'])
                    if not actions:
                        actions.append('Did not add/remove/modify any nonempty files.')
                    info = {'name' : c['author']['name'],
                            'email' : c['author']['email'],
                            'message' : c['message'],
                            'timestamp' : dateutil.parser.parse(c['timestamp']).strftime('%F %T %z'),
                            'actions' : ''.join(actions),
                            'url' : c['url']}
                    
                    msg = """%(url)s
Author: %(name)s <%(email)s>
Date:   %(timestamp)s

%(message)s
---
%(actions)s""" % info
                    zephyr(sender, opts['class'], inst, zsig, msg)
                msg = 'Thanks for posting!'
            else:
                msg = ('If you had sent a POST request to this URL, would have sent'
                       ' a zephyr to -c %s' % opts['class'])
            return msg

    github = Github()
    
    class Default(object):
        @cherrypy.expose
        def default(self, *args, **query):
            try:
                return self._default(*args, **query)
            except Exception, e:
                logger.error('Caught exception %s:\n%s' % (e, traceback.format_exc()))
                raise

        def _default(self, *args, **query):
            logger.info('A %s request with args: %r and query: %r' %
                        (cherrypy.request.method, args, query))
            if 'class' not in query or 'instance' not in query:
                raise cherrypy.HTTPError(400, 'Must specify a zephyr class and instance')
            logger.debug('Specified a class')
            if cherrypy.request.method == 'POST':
                logger.debug('About to load data')
                zsig = query.get('zsig', 'zcommit')
                msg = query.get('message', 'Hello, world!')
                sender = 'daemon.zcommit'
                logger.debug('Set zsig')
                zephyr(sender, query['class'], query['instance'], zsig, msg)
                msg = 'Thanks for posting!'
            else:
                msg = ('If you had sent a POST request to this URL, would have sent'
                       ' a zephyr to -c %s' % query['class'])
            return msg

    default = Default()

def main():
    app = cherrypy.tree.mount(Application(), '/zcommit')
    cherrypy.server.unsubscribe()
    cherrypy.engine.start()
    try:
        WSGIServer(app, environ={'SCRIPT_NAME' : '/zcommit'}).run()
    finally:
        cherrypy.engine.stop()

if __name__ == '__main__':
    sys.exit(main())
