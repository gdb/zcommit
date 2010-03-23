#!/usr/bin/python

import cherrypy
from flup.server.fcgi import WSGIServer
import subprocess
import sys

def zephyr(klass, instance, zsig, msg):
    # TODO: spoof the sender
    cmd = ['zwrite', '-c', klass, '-i', instance,
           '-s', zsig, '-d', '-m', msg]
    subprocess.check_call(cmd)

class Application(object):
    @cherrypy.expose
    def index(self):
        return 'Hello world!'

    @cherrypy.expose
    def default(self, *args, **kwargs):
        return 'hello'

    class Github(object):
        @cherrypy.expose
        def default(self, *args, **query):
            opts = {}
            if len(args) % 2:
                raise cherrypy.HTTPError(400, 'Invalid submission URL')
            for i in xrange(0, len(args), 2):
                opts[args[i]] = args[i + 1]

            if 'class' not in opts:
                raise cherrypy.HTTPError(400, 'Must specify a zephyr class name')

            if cherrypy.request.method == 'POST':
                payload = json.loads(query['payload'])

                zsig = payload['ref']
                if 'zsig' in opts:
                    zsig = '%s: %s' % (opts['zsig'], zsig)
                else:
                    zsig = 'zcommit bot'

                for c in reversed(payload['commits']):
                    inst = opts.get('instance', c['id'][:8])
                    c['added_as_str'] = '\n'.join(c['added'])
                    msg = """%(name)s <%(email)s>
%(message)s
%(timestamp)s
--
%(added)s""" % c
                    zephyr(opts['class'], inst, zsig, msg)
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
