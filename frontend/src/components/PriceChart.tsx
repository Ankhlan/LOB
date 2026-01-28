import { useEffect, useRef, useState } from 'react';
import { createChart, ColorType, IChartApi, ISeriesApi, UTCTimestamp, CandlestickData } from 'lightweight-charts';
import { api } from '../api';
import type { Quote } from '../api/types';
import './PriceChart.css';

interface PriceChartProps {
  symbol: string | null;
}

interface OHLCBar {
  time: UTCTimestamp;
  open: number;
  high: number;
  low: number;
  close: number;
}

export function PriceChart({ symbol }: PriceChartProps) {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const seriesRef = useRef<ISeriesApi<'Candlestick'> | null>(null);
  const [bars, setBars] = useState<OHLCBar[]>([]);
  const lastBarRef = useRef<OHLCBar | null>(null);

  // Create chart on mount
  useEffect(() => {
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      layout: {
        background: { type: ColorType.Solid, color: '#1e1e2e' },
        textColor: '#cdd6f4',
      },
      grid: {
        vertLines: { color: '#313244' },
        horzLines: { color: '#313244' },
      },
      crosshair: {
        mode: 0,
      },
      rightPriceScale: {
        borderColor: '#45475a',
      },
      timeScale: {
        borderColor: '#45475a',
        timeVisible: true,
      },
      width: chartContainerRef.current.clientWidth,
      height: 300,
    });

    const candlestickSeries = chart.addCandlestickSeries({
      upColor: '#a6e3a1',
      downColor: '#f38ba8',
      borderDownColor: '#f38ba8',
      borderUpColor: '#a6e3a1',
      wickDownColor: '#f38ba8',
      wickUpColor: '#a6e3a1',
    });

    chartRef.current = chart;
    seriesRef.current = candlestickSeries;

    // Handle resize
    const handleResize = () => {
      if (chartContainerRef.current) {
        chart.applyOptions({ width: chartContainerRef.current.clientWidth });
      }
    };

    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
  }, []);

  // Subscribe to quotes and build candlesticks
  useEffect(() => {
    if (!symbol) return;

    // Generate some initial bars from current time
    const now = Math.floor(Date.now() / 1000) as UTCTimestamp;
    const initialBars: OHLCBar[] = [];

    // Create 50 empty minute bars
    for (let i = 50; i >= 1; i--) {
      const time = (now - i * 60) as UTCTimestamp;
      initialBars.push({
        time,
        open: 0,
        high: 0,
        low: 0,
        close: 0,
      });
    }

    setBars(initialBars);
    lastBarRef.current = null;

    const unsubscribe = api.subscribeToQuotes((quotes) => {
      const q = quotes.find(q => q.symbol === symbol);
      if (!q || !seriesRef.current) return;

      const mid = (q.bid + q.ask) / 2;
      const currentTime = Math.floor(Date.now() / 1000);
      const barTime = (currentTime - (currentTime % 60)) as UTCTimestamp;

      setBars(prevBars => {
        const newBars = [...prevBars];
        const lastBar = newBars[newBars.length - 1];

        if (lastBar && lastBar.time === barTime) {
          // Update existing bar
          lastBar.high = Math.max(lastBar.high, mid);
          lastBar.low = lastBar.low === 0 ? mid : Math.min(lastBar.low, mid);
          lastBar.close = mid;
        } else {
          // Create new bar
          const newBar: OHLCBar = {
            time: barTime,
            open: mid,
            high: mid,
            low: mid,
            close: mid,
          };
          newBars.push(newBar);

          // Keep last 100 bars
          if (newBars.length > 100) {
            newBars.shift();
          }
        }

        // Update chart
        const validBars = newBars.filter(b => b.close > 0);
        if (validBars.length > 0 && seriesRef.current) {
          seriesRef.current.setData(validBars as CandlestickData<UTCTimestamp>[]);
        }

        return newBars;
      });
    });

    return () => unsubscribe();
  }, [symbol]);

  if (!symbol) {
    return (
      <div className="price-chart">
        <div className="chart-header">
          <h2>📈 Chart</h2>
        </div>
        <div className="chart-empty">
          Select an instrument to view chart
        </div>
      </div>
    );
  }

  return (
    <div className="price-chart">
      <div className="chart-header">
        <h2>📈 {symbol}</h2>
        <span className="chart-timeframe">1m</span>
      </div>
      <div className="chart-container" ref={chartContainerRef} />
    </div>
  );
}
