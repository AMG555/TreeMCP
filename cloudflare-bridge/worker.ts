/**
 * Cloudflare Remote MCP Bridge for TreeMCP
 * Connects remote AI agents (Cloud Claude, ChatGPT Web, Cron Workers) to local TreeMCP instance
 */

export interface Env {
  LOCAL_TREEMCP_TUNNEL_URL?: string; // e.g. https://treemcp.yourdomain.com via Cloudflare Tunnel
  AUTH_TOKEN?: string;
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);

    // Health check
    if (url.pathname === '/' || url.pathname === '/health') {
      return new Response(JSON.stringify({
        status: 'online',
        service: 'TreeMCP Cloudflare Remote Bridge',
        version: '1.0.0',
        transport: 'SSE & HTTP JSON-RPC'
      }), {
        headers: { 'Content-Type': 'application/json' }
      });
    }

    // Auth verification
    if (env.AUTH_TOKEN) {
      const auth = request.headers.get('Authorization');
      if (auth !== `Bearer ${env.AUTH_TOKEN}`) {
        return new Response('Unauthorized', { status: 401 });
      }
    }

    // Relay MCP JSON-RPC requests to local TreeMCP instance through Cloudflare Tunnel
    if (url.pathname === '/mcp' || url.pathname === '/sse') {
      if (!env.LOCAL_TREEMCP_TUNNEL_URL) {
        // Fallback simulated responder if local tunnel is not currently connected
        if (request.method === 'POST') {
          const body: any = await request.json();
          if (body.method === 'initialize') {
            return new Response(JSON.stringify({
              jsonrpc: '2.0',
              id: body.id,
              result: {
                protocolVersion: '2024-11-05',
                capabilities: { tools: {} },
                serverInfo: { name: 'TreeMCP-Cloudflare-Bridge', version: '1.0.0' }
              }
            }), { headers: { 'Content-Type': 'application/json' } });
          }
          if (body.method === 'tools/list') {
            return new Response(JSON.stringify({
              jsonrpc: '2.0',
              id: body.id,
              result: {
                tools: [
                  {
                    name: 'treemcp_drive_summary',
                    description: 'Get remote summary of drives via Cloudflare tunnel.',
                    inputSchema: { type: 'object', properties: {} }
                  }
                ]
              }
            }), { headers: { 'Content-Type': 'application/json' } });
          }
        }
        return new Response(JSON.stringify({
          error: 'LOCAL_TREEMCP_TUNNEL_URL not configured in worker environment.'
        }), { status: 503, headers: { 'Content-Type': 'application/json' } });
      }

      // Forward request to Local TreeMCP instance via Cloudflare Tunnel
      const localTarget = `${env.LOCAL_TREEMCP_TUNNEL_URL}${url.pathname}${url.search}`;
      return fetch(localTarget, {
        method: request.method,
        headers: request.headers,
        body: request.body
      });
    }

    return new Response('Not Found', { status: 404 });
  }
};

