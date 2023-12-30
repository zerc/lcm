import httpx
from aiohttp import web

BASE_URL = 'http://192.168.1.97'

routes = web.RouteTableDef()

@routes.get('/')
async def index(request):
    with open('index.html', 'r') as f:
        return web.Response(text=f.read(), content_type="text/html")

@routes.route('*', '/api/{path:.*?}')
async def api_proxy(request):
    path = request.match_info['path']
    method = request.method.lower()
    async with httpx.AsyncClient(timeout=60) as client:
        response = await getattr(client, method)(f'{BASE_URL}/api/{path}')
        return web.Response(text=response.content.decode(), content_type=response.headers['Content-Type'], status=response.status_code)


app = web.Application()
app.add_routes(routes)
web.run_app(app, port=8000)