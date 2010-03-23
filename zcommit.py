#!/usr/bin/python

import cherrypy
from flup.server.fcgi import WSGIServer
import logging
import json
import subprocess
import sys
import traceback

LOG_FILENAME = 'logs/zcommit.log'

# Set up a specific logger with our desired output level
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Add the log message handler to the logger
handler = logging.FileHandler(LOG_FILENAME)
logger.addHandler(handler)

def zephyr(klass, instance, zsig, msg):
    # TODO: spoof the sender
    logger.info("""About to send zephyr:
class: %(klass)s
instance: %(instance)s
zsig: %(zsig)s
msg: %(msg)s""" % {'klass' : klass, 'instance' : instance,
                 'zsig' : zsig, 'msg' : msg})
    cmd = ['zwrite', '-c', klass, '-i', instance,
           '-s', zsig, '-d', '-m', msg]
    subprocess.check_call(cmd)

class Application(object):
    @cherrypy.expose
    def index(self):
        logger.debug('Hello world app reached')
        return 'Hello world!'

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
                opts[args[i]] = args[i + 1]
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
                logger.debug('Set zsig')
                for c in reversed(payload['commits']):
                    inst = opts.get('instance', c['id'][:8])
                    info = {'name' : c['author']['name'],
                            'email' : c['author']['email'],
                            'message' : c['message'],
                            'timestamp' : c['timestamp'],
                            'added' :  '\n'.join(c['added'])}
                    msg = """%(name)s <%(email)s>
%(message)s
%(timestamp)s
--
%(added)s""" % info
                    zephyr(opts['class'], inst, zsig, msg)
                msg = 'Thanks for posting!'
            else:
                msg = ('If you had sent a POST request to this URL, would have sent'
                       ' a zepyhr to -c %s' % opts['class'])
            return msg

    github = Github()

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
